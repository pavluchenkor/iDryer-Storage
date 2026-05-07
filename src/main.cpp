// iDryer Storage Link — main.cpp.
//
// Тонкий composition root через iDryer::Link фасад. Образцовый Arduino-style
// пример простого устройства линейки iDryer:
//   • один unit;
//   •   периферия — адресная LED-лента (1-wire chipsets: WS2812B/WS2811/
//                  WS2813/WS2815/SK6812-RGB) + фоновые анимации;
//   • sensor     — опциональный SHT31 (температура + влажность);
//   • меню       — генерируется из lib/idryer-menu/menu_v2.yaml;
//   • облако     — WiFi → MQTT → portal через iDryer::Link;
//   • LAN-app    — WebSocket-клиент видит то же что и облако (один publish на оба).

#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>
#include <ArduinoJson.h>

#include <iDryer.h>
#include <local_access/device_publisher.h>
#include <runtime/idryer_runtime.h>

// Меню v3: сгенерированные артефакты из lib/idryer-menu (yaml → C++).
#include <menu_state.h>
#include <menu_ids.h>
#include <menu_bindings.h>     // menu_sync_state_to_cache
#include <menu_commands.h>     // menu_buildFullJson, MENU_FULL_JSON_BUF_SIZE
#include <menu_nvs_io.h>       // menu_nvs_begin, NVS_KEY_*

// Always include WiFi for diagnostic dump (RSSI, IP).
#include <WiFi.h>

#include "storage/led_strip/led_strip_executor.h"
#include "storage/led_strip/led_strip_menu.h"
#include "storage/led_strip/led_strip_animations.h"
#include "storage/sensors/Sht31ClimateSensor.h"

// ── Hardware pins / sizes ────────────────────────────────────────────
#ifndef STORAGE_MAX_LEDS
#define STORAGE_MAX_LEDS 300
#endif
#ifndef STORAGE_LED_PIN
#define STORAGE_LED_PIN 4
#endif
#ifndef STORAGE_I2C_SDA
#define STORAGE_I2C_SDA 8
#endif
#ifndef STORAGE_I2C_SCL
#define STORAGE_I2C_SCL 9
#endif

// ── Hardware: LED strip + climate sensor ─────────────────────────────
static CRGB                s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor    s_executor(s_leds, STORAGE_MAX_LEDS);
static Sht31ClimateSensor  s_sensor(&Wire);
static bool                s_sensorOk = false;

// ── Device facade ────────────────────────────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,

    .hasAirTemp        = true,    // SHT31 (опционально, при отсутствии — 0)
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,

    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,

    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,       // Storage не публикует status

    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link s_link(CFG);

// ── FastLED initialisation ───────────────────────────────────────────
//
// 1-wire chipsets only (data, без clock). 5 чипсетов × 6 color orders = 30 веток.
// FastLED.addLeds<CHIPSET, PIN, ORDER>() — compile-time template, инстанциация
// каждой комбинации. Через DCE неиспользуемые сжимаются.
//
// Изменение chipset / color_order в меню требует перезагрузки устройства —
// FastLED.addLeds<> можно вызвать только один раз за boot.
//
// chipset: 0=WS2812B 1=WS2811 2=WS2813 3=WS2815 4=SK6812
// order:   0=RGB     1=RBG    2=GRB    3=GBR    4=BRG    5=BGR

#define ADD_CHIPSET_ORDERED(CHIP, COUNT, ORDER) \
    case ORDER: FastLED.addLeds<CHIP, STORAGE_LED_PIN, ORDER>(s_leds, COUNT); break

#define ADD_CHIPSET(CHIP, ORDER_IDX, COUNT)                              \
    do {                                                                  \
        switch (ORDER_IDX) {                                              \
            ADD_CHIPSET_ORDERED(CHIP, COUNT, RGB);                        \
            ADD_CHIPSET_ORDERED(CHIP, COUNT, RBG);                        \
            ADD_CHIPSET_ORDERED(CHIP, COUNT, GRB);                        \
            ADD_CHIPSET_ORDERED(CHIP, COUNT, GBR);                        \
            ADD_CHIPSET_ORDERED(CHIP, COUNT, BRG);                        \
            ADD_CHIPSET_ORDERED(CHIP, COUNT, BGR);                        \
            default: FastLED.addLeds<CHIP, STORAGE_LED_PIN, GRB>(s_leds, COUNT); break; \
        }                                                                 \
    } while (0)

static EOrder colorOrderEnum(uint8_t idx) {
    switch (idx) {
        case 0: return RGB;
        case 1: return RBG;
        case 2: return GRB;
        case 3: return GBR;
        case 4: return BRG;
        case 5: return BGR;
        default: return GRB;
    }
}

static void initLedStrip(uint8_t chipset, uint8_t order, uint16_t count) {
    EOrder o = colorOrderEnum(order);
    (void)o;   // используется внутри switch'ей по индексу — макрос подхватывает
    switch (chipset) {
        case 0: ADD_CHIPSET(WS2812B, order, count); break;
        case 1: ADD_CHIPSET(WS2811,  order, count); break;
        case 2: ADD_CHIPSET(WS2813,  order, count); break;
        case 3: ADD_CHIPSET(WS2815,  order, count); break;
        case 4: ADD_CHIPSET(SK6812,  order, count); break;
        default: ADD_CHIPSET(WS2812B, order, count); break;
    }
    FastLED.clear(true);
}

// ── Menu NVS bootstrap ───────────────────────────────────────────────
static void bootstrapMenu() {
    if (!menu_nvs_begin()) {
        HAL_LOG_WARN("MENU", "NVS begin failed — будут только дефолты в RAM");
    }
    menu.initDefaults();

    uint32_t magic = 0, ver = 0;
    ee_read(NVS_KEY_MAGIC, magic);
    ee_read(NVS_KEY_VERSION, ver);
    if (magic != NVS_MENU_MAGIC || ver != (uint32_t)NVS_MENU_VERSION) {
        ee_write(NVS_KEY_MAGIC, (uint32_t)NVS_MENU_MAGIC);
        ee_write(NVS_KEY_VERSION, (uint32_t)NVS_MENU_VERSION);
        HAL_LOG_INFO("MENU", "NVS header initialized (magic=0x%08X ver=%u)",
                     (unsigned)NVS_MENU_MAGIC, (unsigned)NVS_MENU_VERSION);
    }
    menu.loadFromNVS();
    normalizeMenuGroups();
    // Bootstrap sync MenuState→g_menu_cache. Без этого первый commands/get_config
    // отдаст дефолтные значения вместо реально загруженных из NVS.
    menu_sync_state_to_cache();
}

// ── После любого изменения меню — синхронизировать executor + animations ──
static void onMenuChanged() {
    // Defaults для led.pulse: цвет и длительность из меню.
    s_executor.setDefaultColor(selectedPulseColor());
    s_executor.setDefaultDurationSec(pulseDefaultDurationSec());
    // Анимации: enabled/animation/color/яркость.
    animationsApply();
}

// ── Полный config из меню → MQTT + Local WS ──────────────────────────
// Один вызов — два транспорта (s_link.devicePublisher() — dual-publish helper).
static void publishFullMenu() {
    static char buf[MENU_FULL_JSON_BUF_SIZE];
    size_t len = menu_buildFullJson(buf, sizeof(buf));
    if (len == 0) {
        HAL_LOG_ERROR("MENU", "menu_buildFullJson failed");
        return;
    }
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}

// ── Регистрация продуктовых команд через onCommand ───────────────────
// Built-in команды (link_integration / bambu_apply / ping) обрабатывает
// либа сама — здесь только продуктовые имена.
static void registerCommands() {
    s_link.onCommand("get_config", [](JsonObjectConst) {
        publishFullMenu();
    });

    s_link.onCommand("set", [](JsonObjectConst data) {
        int id = data["id"] | -1;
        int val = -1;
        if      (data["val"].is<bool>())  val = data["val"].as<bool>() ? 1 : 0;
        else if (data["val"].is<int>())   val = data["val"].as<int>();
        else if (data["val"].is<float>()) val = (int)data["val"].as<float>();

        if (id >= 0 && val >= 0 &&
            applyConfigChange(id, val, s_executor, onMenuChanged)) {
            // Повторно публикуем full config — UI видит изменения без запроса.
            publishFullMenu();
        } else {
            HAL_LOG_WARN("MAIN", "set ignored: id=%d val=%d (bad/unsupported)", id, val);
        }
    });

    s_link.onCommand("invoke", [](JsonObjectConst data) {
        // TODO menu_protocol_v1: унифицировать с iHeater Link ::applyInvokeCommand
        // (invoke по menu id с args). Сейчас Storage использует action-by-name
        // через ActionDispatcher — продуктовый механизм для LED-команд с args.
        // Будет объединено когда добавим args в MenuItem.action (на этапе
        // миграции сушилки). См. ___capabilities_and_menu_as_protocol.md §10.2.
        const char* action = data["action"] | "";
        if (strcmp(action, "device.getConfig") == 0) {
            publishFullMenu();
            return;
        }
        // led.pulse, led.animation — LED-лента знает свой набор action'ов.
        s_executor.execute(action, data["args"]);
    });
}

// ─── REPL (dev only) ────────────────────────────────────────────────────
#ifdef IDRYER_DEV_REPL

static void printHelp() {
  Serial.println(F("\n=== iDryer dev REPL ==="));
  Serial.println(F("  help                           — this list"));
  Serial.println(F("  wifi <ssid> <password>         — set creds & connect"));
  Serial.println(F("  status                         — wifi/online/serial"));
  Serial.println(F("  claim                          — request claim flow"));
  Serial.println(F("  wipe                           — erase NVS + reboot"));
  Serial.println(F("  restart                        — soft reboot"));
  Serial.println(F("======================="));
}

static void cmdStatus() {
  Serial.printf("[status] wifi=%d ip=%s rssi=%d online=%d serial=%s\n",
                (int)WiFi.status(),
                WiFi.localIP().toString().c_str(),
                WiFi.RSSI(),
                s_link.isOnline() ? 1 : 0,
                s_link.serial());
}

// Single line of input → command dispatch.
static void runCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.printf("> %s\n", line.c_str());

  if (line.equalsIgnoreCase("help") || line == "?") {
    printHelp();
    return;
  }
  if (line.equalsIgnoreCase("status")) {
    cmdStatus();
    return;
  }
  if (line.equalsIgnoreCase("claim") || line.equalsIgnoreCase("START_CLAIM")) {
    // Match prod-side flasher-portal protocol: emit CLAIM_STARTED / CLAIM_ALREADY.
    if (s_link.isOnline()) {
      Serial.printf("CLAIM_ALREADY:%s\n", s_link.serial());
    } else {
      bool ok = s_link.requestClaim();
      Serial.println(ok ? "CLAIM_STARTED:OK" : "CLAIM_STARTED:ERROR");
    }
    Serial.flush();
    return;
  }
  if (line.equalsIgnoreCase("wipe")) {
    Serial.println("[wipe] erasing NVS + reboot…");
    Serial.flush();
    s_link.eraseClaimAndRestart();   // does not return
    return;
  }
  if (line.equalsIgnoreCase("restart")) {
    Serial.println("[restart] reboot…");
    Serial.flush();
    delay(100);
    ESP.restart();
    return;
  }
  if (line.startsWith("wifi ") || line.startsWith("WIFI ")) {
    int sp1 = line.indexOf(' ');
    int sp2 = line.indexOf(' ', sp1 + 1);
    if (sp2 < 0) {
      Serial.println("[wifi] usage: wifi <ssid> <password>");
      return;
    }
    String ssid = line.substring(sp1 + 1, sp2);
    String pass = line.substring(sp2 + 1);
    Serial.printf("[wifi] saving '%s' / '****'\n", ssid.c_str());
    s_link.setWifiCredentials(ssid.c_str(), pass.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    return;
  }

  Serial.printf("[?] unknown command: %s  (type 'help')\n", line.c_str());
}

#endif // IDRYER_DEV_REPL

// ─────────────────────────────────────────────────────────────────────

void setup() {
    // 1. Меню v3: NVS + дефолты + загрузка + нормализация toggle-групп.
    //    ДО initLedStrip — нужен chipset / color_order.
    bootstrapMenu();

    // 2. LED strip: одноразовая FastLED-инициализация.
    uint16_t ledCount = (menu.led_count > STORAGE_MAX_LEDS)
                        ? STORAGE_MAX_LEDS
                        : menu.led_count;
    initLedStrip(selectedChipset(), selectedColorOrder(), ledCount);
    applyMenuToExecutor(s_executor);
    s_executor.setDefaultColor(selectedPulseColor());
    s_executor.setDefaultDurationSec(pulseDefaultDurationSec());

    // 3. Фоновые анимации.
    animationsWire(&s_executor);
    animationsApply();

    // 4. SHT31: опциональный — устройство работает и без него.
    Wire.begin(STORAGE_I2C_SDA, STORAGE_I2C_SCL);
    s_sensorOk = s_sensor.begin();

    // 5. Поднимаем стек: WiFi → claim → MQTT → telemetry/status автомат.
    s_link.begin();

    // Команды портала / Local-WS — единый путь через onCommand.
    // get_config / set / invoke — продуктовые. Built-in (link_integration,
    // bambu_apply, ping) обрабатывает либа сама.
    registerCommands();

#ifdef IDRYER_DEV_REPL
    Serial.println(F("\n[boot] iDryer dev REPL ready — type 'help'"));
#endif
}

void loop() {
    s_link.loop();        // фасад: WiFi/MQTT/LocalAccess + auto-telemetry
    s_executor.loop();    // off-by-timer для led.pulse

    // Фоновая анимация работает только когда нет активного pulse (она это
    // проверяет сама внутри animationsLoop).
    animationsLoop(millis());

    if (s_sensorOk) {
        s_sensor.tick(millis());
        SensorReading r = s_sensor.get();
        if (r.ok) {
            // Фасад читает эти поля по cfg.telemetryPeriodMs (10 сек) и
            // публикует units[{unitId:"U1", temperature, humidity}] +
            // rssi + uptime в MQTT и Local WS.
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;
        }
    }

#ifdef IDRYER_DEV_REPL
    // REPL on Serial. Improv is compiled out in this env.
    // Triggers a command on any of:
    //   - explicit CR or LF line terminator,
    //   - idle timeout (no new byte for 120 ms) — covers terminals
    //     configured to send "no line ending".
    static String   buf;
    static uint32_t lastByteMs = 0;

    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        lastByteMs = millis();
        if (c == '\r' || c == '\n') {
            if (buf.length() > 0) { runCommand(buf); buf = ""; }
            continue;
        }
        buf += c;
        if (buf.length() > 200) buf = "";   // overflow guard
    }

    if (buf.length() > 0 && millis() - lastByteMs > 120) {
        runCommand(buf);
        buf = "";
    }
#endif
}
