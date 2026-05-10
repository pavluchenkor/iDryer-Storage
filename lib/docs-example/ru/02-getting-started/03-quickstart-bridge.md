# Quickstart: свой сетевой модуль (LINK)

Минимальный путь от пустого проекта в PlatformIO до «ESP32-мост принимает Hello от MCU по UART». Здесь уже используется библиотека целиком — зачем писать парсер вручную, когда есть готовый.

!!! note "Контекст"
    Этот квикстарт ведёт **только до первого UART-обмена** с MCU. Подключение к облаку, MQTT, claiming — в следующем квикстарте (standalone) и в разделах [../04-mqtt/](../04-mqtt/), [../05-cloud/](../05-cloud/), [../06-flows/](../06-flows/).
    Если вам нужна полная цепочка UART + облако, последовательно пройдите [02-quickstart-controller.md](02-quickstart-controller.md), этот документ, [04-quickstart-standalone.md](04-quickstart-standalone.md).

!!! warning "Нужны базовые концепты"
    В коде ниже активно используются: `namespace` / `using namespace`, ссылки `const T&`, `enum class`, колбэки (передача функций как параметров), лямбды. Если что-то непонятно — [prerequisites](../01-overview/05-prerequisites.md) §5–§9.

---

## Цель квикстарта

ESP32 слушает UART, принимает от MCU `Hello`, отвечает `HelloAck`, печатает всю входящую телеметрию в Serial Monitor. Дальше остаётся добавить WiFi и MQTT — это отдельные темы.

---

## Что понадобится

**Железо:**

- Плата с ESP32 (любая: DevKit, NodeMCU-32, WROOM, S3, …).
- MCU, уже шлющий `Hello` и `Telemetry` (готовый iDryer-контроллер или ваша плата из [02-quickstart-controller.md](02-quickstart-controller.md)).

**Софт:**

- VSCode с расширением **PlatformIO** (или PIO CLI).
- Git — библиотека подключается по URL.

**Подключение UART:**

```
   MCU                ESP32
  ┌───┐              ┌───┐
  │TX ├──────────────┤GPIO16 (RX1)│
  │RX ├──────────────┤GPIO17 (TX1)│
  │GND├──────────────┤GND│
  └───┘              └───┘
```

---

## Шаг 1. Создать проект PlatformIO

Создайте пустой проект на плату ESP32, затем замените `platformio.ini`:

```ini
[env:esp32dev]
platform    = espressif32
board       = esp32dev
framework   = arduino
monitor_speed = 115200

lib_deps =
  https://github.com/pavluchenkor/idryer-protocol.git
  bblanchon/ArduinoJson @ ^7.0.4
```

`ArduinoJson` понадобится позже — при публикации в MQTT.

---

## Шаг 2. Скетч `src/main.cpp`

Минимальный мост: принимает `Hello`, отвечает `HelloAck`, печатает всё входящее.

```cpp
#include <Arduino.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace DryerUart;
using namespace idryer::hal;

// ============================================================================
// UART к MCU
// ============================================================================
constexpr int UART_RX_PIN = 16;  // ESP32 принимает
constexpr int UART_TX_PIN = 17;  // ESP32 отправляет

// HAL-обёртка над Serial1 (UART_NUM_1)
ArduinoSerial uartSerial(Serial1, /*uartNum*/ 1);
UartBridge    uartBridge;

uint32_t lastHeartbeatMs = 0;

// ============================================================================
// Обработчики входящих кадров
// ============================================================================

void onHello(const HelloPayload &p, const FrameHeader &h) {
    Serial.printf("[UART] Hello: role=%u, fw=%u.%u.%u, hw=%s, units=%u\n",
                  (unsigned)p.role,
                  (p.firmwareVersion >> 16) & 0xFF,
                  (p.firmwareVersion >> 8)  & 0xFF,
                  p.firmwareVersion         & 0xFF,
                  p.hardwareVersion,
                  p.unitsCount);
    Serial.printf("       mcuSerial=%s, deviceType=%u\n",
                  p.mcuSerial, p.deviceType);

    // Отвечаем HelloAck — IP/SSID нулевые, WiFi ещё не поднят
    HelloAckPayload ack{};
    ack.ipAddress = 0;
    ack.ssid[0]   = '\0';
    uartBridge.sendHelloAck(ack);
}

void onTelemetry(const TelemetryPayload &p, const FrameHeader &h) {
    for (uint8_t i = 0; i < p.count; i++) {
        const auto &u = p.units[i];
        Serial.printf("[UART] Telemetry: U%u T=%.1f°C H=%.1f%% heater=%u%% fan=%s\n",
                      u.unitId + 1,
                      u.temperatureC10 / 10.0f,
                      u.humidityPct10 / 10.0f,
                      u.heaterPowerPct,
                      u.fanOn ? "on" : "off");
    }
    uartBridge.sendTelemetryAck(h.sequence);
}

void onHeartbeat(const HeartbeatPayload &p, const FrameHeader &h) {
    Serial.printf("[UART] Heartbeat from MCU: uptime=%us, errors=%u\n",
                  p.uptimeSeconds, p.errorsSinceBoot);
}

void onError(const ErrorPayload &p, bool remote) {
    Serial.printf("[UART] %s error: code=%d seq=%u detail=%u\n",
                  remote ? "remote" : "local",
                  (int)p.code, p.lastSequence, p.detail);
}

// ============================================================================
// Setup & loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n== iDryer UART bridge ==");

    // HAL: логи через Serial
    initArduinoHal(&Serial);

    // UART1 к MCU
    Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    uartBridge.begin(&uartSerial, 115200);

    // Колбэки
    uartBridge.setHelloHandler(onHello);
    uartBridge.setTelemetryHandler(onTelemetry);
    uartBridge.setHeartbeatHandler(onHeartbeat);
    uartBridge.setErrorHandler(onError);

    // Триггерим MCU: «пришли свой Hello с параметрами»
    HelloPayload req{};
    req.role = Role::HelloRequest;     // 0xFF
    uartBridge.sendHello(req);

    Serial.println("Bridge ready. Waiting for MCU...");
}

void loop() {
    uartBridge.loop();

    // Heartbeat MCU раз в 5 секунд (константа из библиотеки)
    if (millis() - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        HeartbeatPayload hb{};
        hb.uptimeSeconds   = millis() / 1000;
        hb.wifiRssiDbm     = 0;           // WiFi ещё не поднят
        hb.errorsSinceBoot = 0;
        hb.cloudState      = 0;           // LinkCloudState::Idle
        uartBridge.sendHeartbeat(hb);
        lastHeartbeatMs = millis();
    }
}
```

---

## Шаг 3. Собрать и залить

В VSCode: `PlatformIO → Build → Upload`, или в терминале:

```bash
pio run -t upload
pio device monitor
```

### Что вы должны увидеть в Serial Monitor

Если MCU работает корректно:

```
== iDryer UART bridge ==
Bridge ready. Waiting for MCU...
[UART] Hello: role=1, fw=1.0.0, hw=v1.0, units=1
       mcuSerial=36B955AB4350FEDC, deviceType=1
[UART] Telemetry: U1 T=55.3°C H=45.2% heater=80% fan=on
[UART] Telemetry: U1 T=55.3°C H=45.2% heater=80% fan=on
[UART] Heartbeat from MCU: uptime=12s, errors=0
...
```

Если видите только `Bridge ready. Waiting for MCU...`:

- проверьте UART-пины (`ESP32 RX ← MCU TX`, `ESP32 TX → MCU RX`),
- проверьте GND (общий обязателен),
- проверьте, что MCU реально запущен и шлёт `Hello`,
- в iDryer Link MCU шлёт Hello только **после** триггера `Role::HelloRequest` — скетч выше этот триггер отправляет в `setup()`.

---

## Что под капотом `UartBridge`

Библиотека делает за вас:

- Парсер: собирает байты в полный кадр, проверяет CRC, отсекает битые и слишком длинные.
- Retry: если вы отправили кадр с `ackRequired = true` и ACK не пришёл за 700 мс, библиотека сама переотправит до 3 раз.
- Фрагментация: для конфигов JSON (`ConfigPush` = 0x30) разбиение на чанки по 194 байта делает библиотека.
- Dispatch: по значению `KIND` вызывается соответствующий handler (`setTelemetryHandler`, `setCommandHandler`, …).

Ваш код — только «что делать при получении» и «что послать».

---

## Что дальше

Теперь у вас ESP32 видит MCU и понимает кадры. Следующий шаг — выход в облако:

1. **WiFi** — подключите ESP32 к точке доступа.
2. **Provision и claim** — первый раз устройство должно получить `deviceToken` и показать пользователю PIN. Стандартный путь автоматизируется через `CloudStateMachine`.
3. **MQTT** — после `Online`-состояния публикуйте телеметрию и статус в стандартные топики.

Всю эту часть делает `CloudStateMachine` + `MqttClient` + `HttpApi` — показано в [04-quickstart-standalone.md](04-quickstart-standalone.md) (там пример без UART, но классы те же самые; для моста добавляется ещё `UartCommandSink` и `CommandHandler`).

### Минимальный чек-лист перед сборкой полного моста

- [ ] Все UART-handler-ы зарегистрированы: Hello, Telemetry, Status, Weights, Rfid, Command, ClaimStart, Heartbeat, Error, Log.
- [ ] При входящей команде от MQTT `CommandHandler` вызывает `UartCommandSink`, а sink — `UartBridge::sendCommand()`.
- [ ] При Hello от MCU сохраняется `mcuSerial` — он нужен для `info` в MQTT.
- [ ] Heartbeat из LINK содержит `cloudState` из `CloudStateMachine::getState()`.
- [ ] При каждом входящем Telemetry публикуется JSON в MQTT (через `TelemetryPublisher` или вручную).
- [ ] LWT MQTT настроен на `idryer/<serial>/offline`.

---

## Полный цикл mass production

Референсная реализация этого моста — прошивка **iDryer Link** ([репозиторий idryer-link](https://github.com/pavluchenkor/idryer-link)). Там всё собрано целиком: экран, меню, staging, обновления по OTA. Используйте как живой образец — это не обязательная зависимость, а рабочий пример на той же библиотеке.

---

← [К списку траекторий](01-choose-your-path.md) | [Standalone на одном ESP32](04-quickstart-standalone.md) →
