# idryer-core

Платформенная библиотека для ESP32-устройств линейки iDryer. Предоставляет общий слой подключения к облаку, MQTT-обмена и маршрутизации команд. Конкретное устройство (Storage Link, iDryer LINK, iHeater LINK) подключает библиотеку и реализует только своё специфичное поведение.

Это библиотека, а не фреймворк. Сборка устройства собирается явно в `main.cpp` продукта — библиотека не прячет её за фасадом.

## Что библиотека делает

- WiFi-подключение и удержание сессии.
- Provisioning (регистрация устройства в бэкенде) и claiming (привязка к аккаунту).
- MQTT-сессия с TLS, persistent session, автореконнект.
- Маршрутизация входящих команд (`commands/invoke`, `commands/set`, `commands/ping`) в продуктовый обработчик.
- Опционально: UART-бридж для двухпроцессорных устройств.
- Опционально: клиенты интеграций — Bambu Lab LAN, Home Assistant MQTT, Moonraker WebSocket.

## Что библиотека не делает

- Не управляет железом продукта (LED-лентами, нагревателями, моторами, датчиками).
- Не публикует телеметрию автоматически — продукт сам решает что и когда публиковать.
- Не содержит бизнес-логики хранения, сушки или нагрева.

## Архитектурная схема

```
                ┌──────────────────────────────────────────────┐
                │              Cloud (idryer.org)              │
                │     HTTPS provisioning  ·  MQTT broker       │
                └──────────────────────────────────────────────┘
                       ▲                            ▲
                  HTTP │                       MQTT │ (TLS)
                       │                            │
┌──────────────────────┴────────────────────────────┴──────────────────┐
│                            idryer-core                               │
│                                                                      │
│   CloudStateMachine                                                  │
│      Idle → WifiConnecting → Provisioning → AwaitingClaim →          │
│      Ready → MqttConnecting → Online                                 │
│                                                                      │
│   IdryerRuntime         single begin() / loop() entry point          │
│      ping handled internally  ·  CommandHandler routes the rest      │
│                                                                      │
│   MqttClient            ActionDispatcher       LocalAccess (LAN WS)  │
│   publishX(...)         invoke / set fallback  optional, port 81     │
│                                                                      │
│   Optional:  UartBridge        LinkIntegrationsManager               │
│              two-MCU           Bambu / HA / Moonraker                │
└──────────────────────────────────────────────────────────────────────┘
                       │ IProfile                  ▲
                       ▼                           │ direct pointers
┌──────────────────────────────┐    ┌──────────────────────────────────┐
│   Product profile            │    │   Product code                   │
│   config / info / lifecycle  │    │   sensors · actuators · publishers│
│   (LedStripProfile, …)       │    │   (Sht31ClimateSensor, LedStrip- │
│                              │    │    Executor, StorageTelemetry-   │
│                              │    │    Publisher, …)                 │
└──────────────────────────────┘    └──────────────────────────────────┘
```

## С чего начать

**Читайте по порядку:**

1. [docs/ru/02-getting-started.md](docs/ru/02-getting-started.md) — короткий вход: что подключить, что прописать, чего ожидать в логе.
2. Запустите **`examples/01_blink_status/`** — самый простой пример. Если LED моргает — стек работает.
3. [docs/ru/02-architecture/01-composition-root.md](docs/ru/02-architecture/01-composition-root.md) — как собирается устройство.
4. [docs/ru/02-architecture/03-data-flow.md](docs/ru/02-architecture/03-data-flow.md) — как движутся данные между участниками.
5. [docs/ru/12-patterns/](docs/ru/12-patterns/) — рецепты: добавить sensor, actuator, transport.

## Примеры (по возрастанию сложности)

| Пример | Что показывает | Зависимости сверх ядра |
|--------|---------------|------------------------|
| [01_blink_status](examples/01_blink_status/01_blink_status.ino) | минимальный composition root, LED моргает в Online | — |
| [minimal_mqtt_only](examples/minimal_mqtt_only/minimal_mqtt_only.ino) | свой `handleCommand`, ActionDispatcher | — |
| [03_with_improv](examples/03_with_improv/03_with_improv.ino) | provisioning WiFi через Improv (без хардкода) | `Improv-WiFi-Library` |
| [mqtt_with_local_ws](examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | LAN WebSocket + dual-publish helper | `WebSockets` |

В начале каждого `.ino` — блок «что показывает / что настроить / Common pitfalls».

Перед сборкой скопируйте [`examples/secrets.h.example`](examples/secrets.h.example) в `include/secrets.h` своего проекта и пропишите свои значения.

## Advanced

- [docs/ru/05-uart/01-uart-layer.md](docs/ru/05-uart/01-uart-layer.md) — UART-бридж для двухпроцессорных устройств (`#include <idryer_uart.h>`).
- [docs/ru/06-integrations/01-integrations-overview.md](docs/ru/06-integrations/01-integrations-overview.md) — клиенты Bambu / Home Assistant / Moonraker (`#include <idryer_integrations.h>`).
- [docs/ru/10-how-to-add-product/01-add-new-product.md](docs/ru/10-how-to-add-product/01-add-new-product.md) — чеклист сборки нового продукта.
- [docs/ru/11-troubleshooting.md](docs/ru/11-troubleshooting.md) — типовые проблемы и их причины.
- Полный индекс документации: [`docs/ru/README.md`](docs/ru/README.md).

## Зависимости

- `ArduinoJson` — сериализация JSON.
- `PubSubClient` — MQTT-транспорт.
- `WebSockets` (Markus Sattler) — для опционального `LocalAccess`.
- `Preferences` — встроенная NVS для credentials и конфигурации.
- `WiFi` / `WiFiClientSecure` — встроенный стек ESP32.

Библиотека не тянет FastLED, Wire, ImprovWiFi, SHT31 — это продуктовые зависимости.

## Терминология

| Термин | Значение |
|--------|----------|
| **profile** | реализация `IProfile` — продуктовый контракт: config, info, lifecycle |
| **sensor** | продуктовый источник данных (датчик) |
| **actuator** | продуктовый исполнитель железа (LED, нагреватель, реле) |
| **transport** | канал обмена с внешним миром: MQTT, локальный WS |
| **publisher** | компонент, который отправляет данные в один или несколько transport (например, `DevicePublisher` — dual-publish helper для MQTT + Local WS) |
| **composition root** | секция `main.cpp`, где статически собирается весь стек устройства |

## Лицензия и поддержка

Внутренняя библиотека iDryer. Для вопросов по интерфейсу и контрактам — `contracts/README.md`.
