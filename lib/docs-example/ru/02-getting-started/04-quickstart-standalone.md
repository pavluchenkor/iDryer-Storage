# Quickstart: один ESP32 за всё (standalone)

Минимальный путь для устройства, где один ESP32 и управляет железом, и общается с облаком. UART между платами нет.

Типичные случаи: модуль телеметрии, iHeater-подобные устройства, простые одноконтурные сушилки.

!!! note "Контекст"
    Перед этим квикстартом имеет смысл понять UART-протокол: структуры `CommandPayload`, `ProfilePayload` — часть контракта облака, и вам придётся «применять» их локально. Быстрый обзор — [../01-overview/03-nodes-and-roles.md](../01-overview/03-nodes-and-roles.md). Детали — [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

---

## Цель квикстарта

ESP32 проходит полный цикл «из коробки → онлайн в облаке»:

1. Подключается к WiFi.
2. Выполняет `provision` (получает `deviceToken`).
3. Получает PIN, пользователь вводит его в приложение.
4. Дожидается завершения claim.
5. Подключается к MQTT.
6. Публикует телеметрию и статус.
7. Принимает команды и применяет их локально (без UART).

Основной мотор — класс `CloudStateMachine`.

---

## Что понадобится

- Плата с ESP32.
- Учётная запись в [portal.idryer.org](https://portal.idryer.org).
- WiFi-сеть с выходом в интернет.
- PlatformIO.

---

## Шаг 1. `platformio.ini`

```ini
[env:esp32dev]
platform   = espressif32
board      = esp32dev
framework  = arduino
monitor_speed = 115200

build_flags =
  -DIDRYER_API_BASE=\"https://portal.idryer.org/api\"

lib_deps =
  https://github.com/pavluchenkor/idryer-protocol.git
  bblanchon/ArduinoJson @ ^7.0.4
```

`IDRYER_API_BASE` задаёт базовый URL REST API (на проде — `https://portal.idryer.org/api`, на localhost backend-а без nginx — `http://localhost:3000`).

---

## Шаг 2. `src/main.cpp`

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace idryer;
using namespace idryer::cloud;

// ============================================================================
// Конфигурация
// ============================================================================
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
const char* API_BASE      = IDRYER_API_BASE;   // задано в platformio.ini

// ============================================================================
// Компоненты
// ============================================================================
ArduinoWifiManager     wifiMgr;
ArduinoHttpClient      httpClient;
ArduinoCredentialStore credStore;

HttpApi            api(&httpClient, API_BASE);
MqttClient         mqtt;
CloudStateMachine  cloud(&wifiMgr, &credStore, &api, &mqtt);

uint32_t lastTelemetryMs = 0;

// ============================================================================
// serialNumber из MAC-адреса ESP32
// (в двух-MCU конфигурации serial приходит от MCU в Hello; здесь MCU нет)
// ============================================================================
void makeSerialFromMac(char *buf, size_t bufSize) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(buf, bufSize, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ============================================================================
// Cloud callbacks
// ============================================================================

void onStateChange(CloudState oldS, CloudState newS, void*) {
    Serial.printf("[CLOUD] %s -> %s\n",
                  cloudStateToString(oldS),
                  cloudStateToString(newS));
}

void onClaimPin(const char *pin, uint32_t expiresIn, void*) {
    Serial.println();
    Serial.println("================================");
    Serial.printf("  PIN: %s\n", pin);
    Serial.printf("  Введите в приложении iDryer\n");
    Serial.printf("  Действителен: %u сек\n", expiresIn);
    Serial.println("================================");
    Serial.println();
}

void onClaimComplete(const char *deviceId, void*) {
    Serial.printf("[CLOUD] Устройство привязано. deviceId=%s\n", deviceId);
}

void onUnclaimed(void*) {
    Serial.println("[CLOUD] Устройство не привязано. Наберите 'claim' в Serial Monitor.");
}

// ============================================================================
// MQTT команды от Backend
// ============================================================================

void onMqttCommand(const char *cmd, JsonObjectConst data) {
    Serial.printf("[MQTT] cmd=%s\n", cmd);

    if (strcmp(cmd, "drying") == 0) {
        uint8_t unitId = data["unitId"] | 0;
        int temp       = data["params"]["temperature"] | 50;
        int minutes    = data["params"]["duration"]    | 60;
        Serial.printf("  -> DRYING U%d  target=%d°C  duration=%d min\n",
                      unitId + 1, temp, minutes);
        // Здесь: запустить PID, таймер, установить target...
    }
    else if (strcmp(cmd, "stop") == 0) {
        Serial.println("  -> STOP");
        // Выключить нагреватель, сбросить таймер...
    }
    else if (strcmp(cmd, "find") == 0) {
        Serial.println("  -> FIND (мигание экрана/LED)");
    }
    // остальные команды — см. docs/04-mqtt/04-backend-to-device.md
}

// ============================================================================
// Публикация телеметрии
// ============================================================================

void publishTelemetry() {
    // Реальные данные подставьте из ваших датчиков
    float temp     = 25.0f + (random(0, 50) / 10.0f);
    float humidity = 40.0f + (random(0, 20));
    uint8_t heater = 0;
    bool fan       = false;

    StaticJsonDocument<256> doc;
    char ts[32];
    doc["timestamp"] = MqttClient::getIsoTimestamp(ts);

    JsonArray units = doc.createNestedArray("units");
    JsonObject u = units.createNestedObject();
    u["unitId"]      = "U1";
    u["temperature"] = temp;
    u["humidity"]    = humidity;
    u["heaterPower"] = heater;
    u["fanStatus"]   = fan;

    mqtt.publishTelemetry(doc);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n== iDryer standalone ==");

    initArduinoHal(&Serial);

    wifiMgr.begin(WIFI_SSID, WIFI_PASSWORD);

    // Cloud колбэки
    cloud.setStateChangeCallback(onStateChange, nullptr);
    cloud.setClaimPinCallback(onClaimPin, nullptr);
    cloud.setClaimCompleteCallback(onClaimComplete, nullptr);
    cloud.setUnclaimedCallback(onUnclaimed, nullptr);

    // MQTT команды
    mqtt.setCommandCallback(onMqttCommand);

    // Standalone: UART нет, serial из MAC
    cloud.setWaitForMcuSerial(false);
    cloud.begin();

    char serial[13];
    makeSerialFromMac(serial, sizeof(serial));
    cloud.setMcuSerial(serial);

    Serial.printf("[INIT] serialNumber: %s\n", serial);
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    cloud.loop();

    // Интерактивный claim через Serial
    if (Serial.available()) {
        String in = Serial.readStringUntil('\n');
        in.trim();
        if (in == "claim") {
            Serial.println("[USER] requestClaim");
            cloud.requestClaim();
        }
    }

    // Публикация — только когда онлайн
    if (!cloud.isOnline()) return;

    uint32_t now = millis();
    if (now - lastTelemetryMs >= 5000) {
        publishTelemetry();
        lastTelemetryMs = now;
    }
}
```

---

## Шаг 3. Запуск и привязка устройства

### 3.1. Первая сборка, заливка

```bash
pio run -t upload
pio device monitor
```

### 3.2. Что происходит по шагам (Serial Monitor)

```
== iDryer standalone ==
[INIT] serialNumber: AABBCCDDEEFF
[CLOUD] Idle -> WifiConnecting
[CLOUD] WifiConnecting -> Provisioning
[CLOUD] Provisioning -> Registering
[CLOUD] Registering -> AwaitingClaim

================================
  PIN: 12345678
  Введите в приложении iDryer
  Действителен: 600 сек
================================
```

### 3.3. Привязка в приложении

1. Залогиньтесь в [portal.idryer.org](https://portal.idryer.org) или в мобильном приложении.
2. «Добавить устройство» → введите PIN `12345678`.
3. Через несколько секунд в Serial Monitor:

```
[CLOUD] AwaitingClaim -> Ready
[CLOUD] Device claimed. deviceId=3fa85f64-5717-4562-b3fc-2c963f66afa6
[CLOUD] Ready -> MqttConnecting
[CLOUD] MqttConnecting -> Online
```

Теперь в портале во вкладке устройства видны ваши `temperature` и `humidity`.

### 3.4. Повторный запуск

После первого claim `deviceToken` сохранён в NVS. При следующем включении устройство сразу идёт: `WifiConnecting → Ready → MqttConnecting → Online`, без PIN.

---

## Команды от Backend

Пока всё, что делает скетч при команде `drying` — печатает параметры в Serial. Следующий шаг — применить их:

```cpp
void onMqttCommand(const char *cmd, JsonObjectConst data) {
    if (strcmp(cmd, "drying") == 0) {
        uint8_t unitId = data["unitId"] | 0;
        float tempC    = data["params"]["temperature"] | 50;
        int durMinutes = data["params"]["duration"]    | 60;

        setTargetTemperature(tempC);          // ваш PID
        startDryingTimer(durMinutes);
        setHeaterEnable(true);
        currentMode = DryerMode::Drying;
    }
    // ...
}
```

Структуру JSON каждой команды см. в [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md).

### Альтернатива: CommandHandler + свой ICommandSink

Вместо прямого парсинга JSON можно использовать готовый `CommandHandler` — он разберёт JSON и вызовет методы вашего `ICommandSink` c готовыми бинарными структурами (`CommandPayload`, `ProfilePayload`). Полезно, если хотите переиспользовать одну и ту же логику применения команд в standalone и в двух-MCU конфигурации. Подробно: [../05-cloud/03-command-sink.md](../05-cloud/03-command-sink.md).

---

## Что публиковать, кроме telemetry

Для полноценного устройства, видимого порталу, минимально нужны:

| Топик | Когда публиковать | Метод |
|-------|-------------------|-------|
| `info` | Один раз после Online (retained) | `mqtt.publishInfo(doc)` |
| `telemetry` | Раз в 5 секунд | `mqtt.publishTelemetry(doc)` |
| `status` | При изменении режима (retained) | `mqtt.publishStatus(doc)` |

Форматы JSON и обязательные поля — [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md).

---

## Чек-лист готовности

- [ ] ESP32 успешно подключается к WiFi.
- [ ] PIN виден в Serial Monitor.
- [ ] После ввода PIN устройство переходит в `Online`.
- [ ] В портале видна последняя телеметрия.
- [ ] Команда `stop` / `drying` из приложения логируется на устройстве.
- [ ] `info` опубликован retained — при перезапуске портала устройство сразу видно.
- [ ] `LWT` (published_by-broker `offline`) работает — при отключении питания в UI появляется «offline».

---

← [К списку траекторий](01-choose-your-path.md) | [UART-протокол подробно](../03-uart/) →
