# RP2040 Dryer Controller Example

Пример использования библиотеки `idryer-protocol` на **RP2040** в роли контроллера сушилки.

## Назначение

RP2040 управляет аппаратной частью сушилки:
- 🌡️ Читает датчики температуры и влажности
- 🔥 Управляет нагревателями
- 💨 Управляет вентиляторами
- 📡 Отправляет телеметрию в ESP32 по UART
- 📥 Получает команды от ESP32 (start, stop, config)

ESP32 в свою очередь:
- 📶 Подключается к WiFi
- ☁️ Отправляет данные в облако через MQTT
- 📲 Получает команды из мобильного приложения
- 🔄 Передает команды на RP2040

## Схема подключения

```
┌──────────────┐         UART          ┌──────────────┐
│   RP2040     │◄─────────────────────►│    ESP32     │
│  Controller  │  115200 baud          │   Bridge     │
└──────────────┘                       └──────────────┘
      │                                        │
      │                                        │
   Датчики,                                WiFi/MQTT
   нагреватели,                              ↕
   вентиляторы                            Backend
```

**Подключение UART:**
- RP2040 GPIO0 (TX) → ESP32 RX
- RP2040 GPIO1 (RX) → ESP32 TX
- GND → GND

## Использованные возможности библиотеки

### 1. UartBridge
Основной класс для UART коммуникации:
```cpp
UartBridge uartBridge;
uartBridge.begin(Serial1, 115200);
```

### 2. Отправка телеметрии
```cpp
TelemetryPayload payload{};
payload.count = 4;
payload.units[0].temperatureC10 = 550;  // 55.0°C
payload.units[0].humidityPct = 35;
uartBridge.sendTelemetry(payload);
```

### 3. Отправка статуса
```cpp
StatusPayload payload{};
payload.uptime = 3600;
payload.units[0].mode = DryerMode::Drying;
payload.units[0].elapsedSeconds = 1800;
uartBridge.sendStatus(payload);
```

### 4. Получение команд от ESP32
```cpp
void handleCommand(const CommandPayload& payload, const FrameHeader& header) {
    if (payload.command == CommandCode::Start) {
        // Запустить сушку
        startDrying(payload.arg0, payload.arg1);
    }
}
uartBridge.setCommandHandler(handleCommand);
```

### 5. Heartbeat
```cpp
HeartbeatPayload payload{};
payload.uptimeSeconds = millis() / 1000;
uartBridge.sendHeartbeat(payload);
```

## Сборка и загрузка

### Arduino IDE

1. Установить поддержку RP2040:
   - Файл → Настройки → Дополнительные ссылки для Менеджера плат:
   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```

2. Инструменты → Плата → Raspberry Pi RP2040 Boards → Raspberry Pi Pico

3. Скопировать библиотеку `idryer-protocol` в `~/Arduino/libraries/`

4. Открыть `rp2040_dryer_controller.ino`

5. Загрузить на плату

### PlatformIO

```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino
lib_deps =
    idryer-protocol
```

```bash
pio run --target upload
```

## Адаптация под реальное железо

В примере используется симуляция. Для реального проекта:

### 1. Датчики температуры/влажности

Замените симуляцию на чтение с реальных датчиков:

```cpp
#include <DHT.h>
DHT dht(DHT_PIN, DHT22);

void readSensors() {
    units[0].temperature = dht.readTemperature();
    units[0].humidity = dht.readHumidity();
}
```

### 2. Управление нагревателями

```cpp
#define HEATER_PIN 2

void setHeaterPower(uint8_t unitId, uint8_t power) {
    analogWrite(HEATER_PIN + unitId, map(power, 0, 100, 0, 255));
}
```

### 3. Управление вентиляторами

```cpp
#define FAN_PIN 6

void setFan(uint8_t unitId, bool on) {
    digitalWrite(FAN_PIN + unitId, on ? HIGH : LOW);
}
```

### 4. RFID считыватели

```cpp
#include <MFRC522.h>
MFRC522 mfrc522(SS_PIN, RST_PIN);

void checkRfid() {
    if (mfrc522.PICC_IsNewCardPresent()) {
        RfidPayload payload{};
        payload.event = RfidEvent::TagDetected;
        payload.readerId = 0;
        payload.unitId = 0;
        // Копируем UID метки
        memcpy(payload.tag, mfrc522.uid.uidByte,
               min(mfrc522.uid.size, sizeof(payload.tag)));

        uartBridge.sendRfid(payload);
    }
}
```

### 5. Весовые датчики

```cpp
#include <HX711.h>
HX711 scale;

void readWeights() {
    WeightsPayload payload{};
    payload.count = 4;

    for (uint8_t i = 0; i < 4; i++) {
        payload.weights[i].sensorId = i;
        payload.weights[i].unitId = i;
        payload.weights[i].weightGramsC10 = scale.get_units();
    }

    uartBridge.sendWeights(payload);
}
```

## Отладка

Включите Serial Monitor (115200 baud) для просмотра логов:

```
[INIT] UART initialized (TX=0, RX=1, 115200 baud)
[INIT] UartBridge configured
[TX] Initial Hello sent to ESP32
[HELLO] ESP32 connected: role=1, fw=0x00010000
[TX] Telemetry sent (4 units)
[CMD] Received command: code=1, state=0, arg0=550, arg1=240
[CMD] Unit 0 started: target=55.0°C, duration=240min
[TX] Status sent (uptime=120s)
```

## API Reference

Полная документация протокола: `/lib/idryer-protocol/docs/`

### Основные структуры данных

- `TelemetryPayload` - телеметрия (температура, влажность)
- `StatusPayload` - статус работы (режим, таймеры)
- `WeightsPayload` - данные весов
- `RfidPayload` - события RFID
- `CommandPayload` - команды управления
- `ConfigPayload` - конфигурация параметров

### Обработчики событий

```cpp
uartBridge.setCommandHandler(handleCommand);      // Команды от ESP32
uartBridge.setConfigHandler(handleConfig);        // Конфигурация
uartBridge.setHelloHandler(handleHello);          // Приветствие
uartBridge.setHeartbeatHandler(handleHeartbeat);  // Heartbeat
uartBridge.setErrorHandler(handleError);          // Ошибки
```

## Лицензия

MIT License
