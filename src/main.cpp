// iDryer Storage Link — точка входа: меню/NVS → LED-лента → SHT31 → SDK begin() → onCommand-обработчики.

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

    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasLed            = true,
    .hasScales         = false,
    .hasRfid           = false,
    .hasAirTemp        = true,    // SHT31 (опционально, при отсутствии — 0)
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,

    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,

    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,       // Storage не публикует status

    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",

    // Название продукта — отображается в колонке «Тип» на странице устройств.
    // Задаётся свободно: любая строка UTF-8.
    .model             = "iDryer Storage",
};

static iDryer::Link s_link(CFG);

// FastLED требует тип чипсета как шаблонный параметр — задаётся один раз при boot (смена требует перезагрузки).
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
    (void)o;   // o не передаётся в макрос — подавление предупреждения компилятора
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
    // Синхронизирует MenuState → g_menu_cache; без этого get_config вернёт дефолты вместо NVS-значений.
    menu_sync_state_to_cache();
}

// Обновляет executor и анимации после изменения параметров меню.
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

// Продуктовые команды портала/Local-WS (get_config/set/invoke). Встроенные (ping и др.) — в либе.
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
        // TODO: унифицировать invoke с iHeater Link (menu id + args) при миграции сушилки.
        const char* action = data["action"] | "";
        if (strcmp(action, "device.getConfig") == 0) {
            publishFullMenu();
            return;
        }
        // led.pulse, led.animation — LED-лента знает свой набор action'ов.
        s_executor.execute(action, data["args"]);
    });

    // Авто-публикация меню при первом выходе в онлайн.
    // Портал подписывается на /config при открытии карточки устройства —
    // retained-сообщение даёт ему актуальное меню без явного get_config.
    s_link.every(2000, []() {
        static bool s_wasOnline = false;
        const bool online = s_link.isOnline();
        if (online && !s_wasOnline) {
            publishFullMenu();
        }
        s_wasOnline = online;
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

// Разбирает команду из Serial и вызывает нужный обработчик.
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
    // Протокол загрузчика: отвечаем CLAIM_ALREADY или CLAIM_STARTED.
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
    // 0. WiFi.persistent(false) — ПЕРВОЙ СТРОКОЙ, до любых NVS-операций.
    //    Иначе Arduino пишет WiFi-config в NVS внутри WiFi.begin() (Improv flow),
    //    конфликтует с открытым menu-NVS-handle и Improv таймаутит.
    WiFi.persistent(false);

    // 1. SDK + Improv — поднимаем РАНО, чтобы handleSerial был готов
    //    перехватить байты от portal до того, как остальной setup (тяжёлый NVS,
    //    FastLED templates, sensor probe) поглотит CPU. Setup должен быть тонким
    //    "сверху" — медленные init идут после.
    s_link.onClaimPin([](const char* pin, uint32_t expires) {
        Serial.printf("CLAIM_PIN:%s:%lu\n", pin, expires);
        Serial.flush();
    });
    s_link.begin();

    // 2. Меню v3: NVS + дефолты + загрузка + нормализация toggle-групп.
    //    ДО initLedStrip — нужен chipset / color_order.
    bootstrapMenu();

    // 3. LED strip.
    uint16_t ledCount = (menu.led_count > STORAGE_MAX_LEDS) ? STORAGE_MAX_LEDS : menu.led_count;
    initLedStrip(selectedChipset(), selectedColorOrder(), ledCount);
    applyMenuToExecutor(s_executor);
    s_executor.setDefaultColor(selectedPulseColor());
    s_executor.setDefaultDurationSec(pulseDefaultDurationSec());

    // 4. Фоновые анимации.
    animationsWire(&s_executor);
    animationsApply();

    // 5. SHT31: опциональный — устройство работает и без него.
    //    begin() теперь lazy (probe в первый tick) — setup остаётся быстрым.
    Wire.begin(STORAGE_I2C_SDA, STORAGE_I2C_SCL);
    s_sensorOk = s_sensor.begin();

    registerCommands();

#ifdef IDRYER_DEV_REPL
    Serial.println(F("\n[boot] iDryer dev REPL ready — type 'help'"));
#endif
}

void loop() {
    s_link.loop();        // фасад: WiFi/MQTT/LocalAccess + auto-telemetry
    s_executor.loop();    // off-by-timer для led.pulse

    animationsLoop(millis());   // не запускается пока активен pulse (проверяет внутри)

    if (s_sensorOk) {
        s_sensor.tick(millis());
        SensorReading r = s_sensor.get();
        if (r.ok) {
            s_link.telemetry.airTempC[0]       = r.temperature;
            s_link.telemetry.airHumidityPct[0] = r.humidity;   // публикуются по telemetryPeriodMs
        }
    }

#ifdef IDRYER_DEV_REPL
    // Разбор ввода: завершение по CR/LF или паузе 120 мс (для терминалов без «конца строки»).
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
        if (buf.length() > 200) buf = "";   // защита от переполнения
    }

    if (buf.length() > 0 && millis() - lastByteMs > 120) {
        runCommand(buf);
        buf = "";
    }
#endif
}
