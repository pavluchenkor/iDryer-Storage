# iDryer Protocol Library

Библиотека протоколов для системы iDryer: UART (RP2040 ↔ ESP32) + MQTT (ESP32 ↔ Backend).

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Возможности

- UART протокол с CRC16, фрагментация, ACK/retry
- MQTT топики для Backend интеграции
- JSON конфиг через UART
- Система ошибок
- Опционально: публикация сущностей Home Assistant (`HaMqttClient`, `HaPublisher`, префикс `homeassistant/…`) — см. `src/mqtt/ha_mqtt_client.*`, `src/cloud/ha_publisher.*`

## Установка

**PlatformIO:**
```ini
lib_deps = https://github.com/pavluchenkor/idryer-protocol.git
```

**Arduino IDE:** Sketch → Include Library → Add .ZIP Library

## Разработчик своего продукта (не свой портал)

Библиотека для устройств, **совместимых с облаком iDryer**. Вы пишете прошивку и логику продукта; портал и MQTT-брокер инфраструктуры — **готовые сервисы**.

**Старт документации:** [docs/00-developer/01-your-product-in-idryer-cloud.md](docs/00-developer/01-your-product-in-idryer-cloud.md) → далее файлы `docs/*/00-for-product-developers.md` по разделам.

Референсная потребительская прошивка **iDryer Link** (железо, меню, staging): [репозиторий Link](https://github.com/pavluchenkor/idryer-link), каталог `docs/guide/` — только справка по этому продукту.

## Документация

| Документ | Описание |
|----------|----------|
| [Оглавление docs](docs/README.md) | Структура документации и вход для разработчика продукта |
| [Реестр расхождений и доработок](docs/protocol-gaps-and-followups.md) | Неточности, незавершённые цепочки, расхождения спеки и кода |
| [UART протокол](docs/02-uart/01-uart.md) | Спецификация UART протокола |
| [MQTT протокол](docs/03-mqtt/01-mqtt.md) | Топики и JSON-форматы |
| [RFID функционал](docs/09-features/01-rfid-functional.md) | Работа с RFID метками |
| [examples/](examples/) | Примеры использования |

## Быстрый старт

### UART Bridge (ESP32 ↔ RP2040)

```cpp
#include <idryer_protocol.h>
#include "hal/hal_arduino.h"

using namespace DryerUart;

ArduinoSerial arSerial(Serial1, 1);
UartBridge uartBridge;

void setup() {
  Serial1.begin(115200, SERIAL_8N1, 16, 17);
  uartBridge.begin(&arSerial, 115200);
  
  uartBridge.setTelemetryHandler([](const TelemetryPayload &p, const FrameHeader &h) {
    Serial.printf("Temp: %.1f°C\n", p.units[0].temperatureC10 / 10.0);
  });
}

void loop() {
  uartBridge.loop();
}
```

### Cloud Standalone (ESP32)

```cpp
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace idryer;

ArduinoWifiManager wifiMgr;
ArduinoHttpClient httpClient;
ArduinoCredentialStore credStore;
HttpApi api(&httpClient, "https://api.idryer.io");
MqttClient mqtt;
CloudStateMachine cloud(&wifiMgr, &credStore, &api, &mqtt);

void setup() {
  initArduinoHal(&Serial);
  wifiMgr.begin("ssid", "password");
  cloud.begin();
}

void loop() {
  cloud.loop();
}
```

## Примеры

| Пример | Описание |
|--------|----------|
| [esp32_standalone](examples/esp32_standalone/) | ESP32 standalone (Single-MCU) |
| [uart_esp32_bridge](examples/uart_esp32_bridge/) | ESP32 мост для RP2040 |
| [rp2040_dryer_controller](examples/rp2040_dryer_controller/) | RP2040 контроллер |

## Архитектура

**Two-MCU:** RP2040 (контроллер) + ESP32 (cloud bridge)  
**Single-MCU:** ESP32 standalone (MAC как serial)

UART слой работает на любом Arduino-совместимом железе.  
Cloud/MQTT слой требует ESP32 (WiFi, HTTPClient, NVS).

## Лицензия

MIT License — Copyright (c) 2025-2026 Ruslan Pavluchenko

**Автор:** pavluchenko.r@gmail.com  
**Версия:** 0.3.0
