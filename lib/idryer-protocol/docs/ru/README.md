# Документация `idryer-protocol`

Библиотека **`idryer-protocol`** закрывает связь в экосистеме iDryer: UART между контроллером и сетевым модулем, MQTT к облаку, HTTP к порталу, разбор команд. Предназначена для **разработчиков, делающих своё устройство, совместимое с облаком iDryer**.

Эта документация — единое руководство. Читайте по порядку, если вы здесь впервые; или сразу идите в нужный раздел, если ищете конкретный справочник.

---

## Как читать

Документы пронумерованы. Вход — слева, глубина — справа.

```
01-overview/          — Суть: что это, кто узлы, словарь терминов
02-getting-started/   — Быстрый старт по вашей траектории
03-uart/              — UART-протокол целиком
04-mqtt/              — MQTT-контракт с облаком
05-cloud/             — HTTP API портала и claiming
06-flows/             — Сквозные сценарии
07-features/          — RFID, локальный WebSocket
```

Если вы первый раз — начинайте с [01-overview/01-what-is-idryer.md](01-overview/01-what-is-idryer.md) и читайте по порядку. Разделы спроектированы так, что каждый опирается только на предыдущие.

!!! tip "Базовых знаний C/C++ / MQTT / веба маловато?"
    Прочитайте [01-overview/05-prerequisites.md](01-overview/05-prerequisites.md) — мини-тьюториал за 10 минут по всему что автор документации предполагает знакомым (битовые операции, little-endian, `#pragma pack`, `enum class`, MQTT QoS/retained/keepalive/LWT, mDNS, JWT и т.п.). Без этой базы раздел [03-uart/](03-uart/) особенно рискует выглядеть как шифр.

---

## Карта документации

### 01. Обзор

| Документ | О чём |
|----------|-------|
| [01-what-is-idryer.md](01-overview/01-what-is-idryer.md) | Что за продукт, кому библиотека, три траектории |
| [02-architecture.md](01-overview/02-architecture.md) | Три узла (MCU / LINK / Cloud), три канала связи |
| [03-nodes-and-roles.md](01-overview/03-nodes-and-roles.md) | Кто что делает, где проходят границы ответственности |
| [04-glossary.md](01-overview/04-glossary.md) | Словарь терминов и идентификаторов |
| [**05-prerequisites.md**](01-overview/05-prerequisites.md) | **Мини-тьюториал**: что нужно знать из C/C++/MQTT/веба перед чтением остальной документации |

### 02. Быстрый старт

| Документ | Ваш сценарий |
|----------|--------------|
| [01-choose-your-path.md](02-getting-started/01-choose-your-path.md) | Выбор траектории из трёх |
| [02-quickstart-controller.md](02-getting-started/02-quickstart-controller.md) | Свой MCU: ручная сборка кадра, CRC, hex-дампы |
| [03-quickstart-bridge.md](02-getting-started/03-quickstart-bridge.md) | Свой ESP32-мост на библиотеке (PlatformIO) |
| [04-quickstart-standalone.md](02-getting-started/04-quickstart-standalone.md) | Один ESP32 за всё, полный цикл до Online |
| [05-dev-claim.md](02-getting-started/05-dev-claim.md) | **Как привязать устройство к аккаунту**: через экран, через веб-инсталлер, через Serial Monitor (dev) |

### 03. UART-протокол

| Документ | Содержание |
|----------|------------|
| [01-physical-layer.md](03-uart/01-physical-layer.md) | Физика: 115200 8N1, схема подключения, level-shifter |
| [02-frame-and-crc.md](03-uart/02-frame-and-crc.md) | Структура кадра, флаги, CRC-16/CCITT-FALSE с кодом |
| [03-message-types.md](03-uart/03-message-types.md) | Таблица всех `MessageKind` со статусами |
| [04-binary-structures.md](03-uart/04-binary-structures.md) | Офсеты, размеры и типы каждой payload-структуры |
| [05-ack-retry.md](03-uart/05-ack-retry.md) | ACK, retry, фрагментация, идемпотентность, `_pad` |
| [06-examples.md](03-uart/06-examples.md) | 14 живых hex-дампов с проверенными CRC |

### 04. MQTT

| Документ | Содержание |
|----------|------------|
| [01-connection.md](04-mqtt/01-connection.md) | Подключение, TLS/CA, credentials, LWT, keepalive, buffer |
| [02-topics.md](04-mqtt/02-topics.md) | Полная таблица топиков с QoS и retained |
| [03-device-to-backend.md](04-mqtt/03-device-to-backend.md) | JSON-форматы публикаций устройства |
| [04-backend-to-device.md](04-mqtt/04-backend-to-device.md) | JSON-команды от облака |
| [05-examples.md](04-mqtt/05-examples.md) | Готовые `mosquitto_pub`/`sub` для отладки |

### 05. Cloud и HTTP

| Документ | Содержание |
|----------|------------|
| [01-claiming-overview.md](05-cloud/01-claiming-overview.md) | Обзор привязки устройства к аккаунту |
| [02-http-api.md](05-cloud/02-http-api.md) | Контракт REST API портала с `curl`-примерами |
| [03-command-sink.md](05-cloud/03-command-sink.md) | `ICommandSink` для UART-моста и standalone |

### 06. Сквозные сценарии

| Документ | Сценарий |
|----------|----------|
| [01-first-boot.md](06-flows/01-first-boot.md) | Первый запуск: boot → Hello → WiFi → claim → MQTT |
| [02-claiming-flow.md](06-flows/02-claiming-flow.md) | Полный цикл привязки с UART-кадрами |
| [03-remote-config.md](06-flows/03-remote-config.md) | Обмен конфигом меню: full / delta / set / invoke |
| [04-profile-mode.md](06-flows/04-profile-mode.md) | Профильная сушка: PID, RAMP/HOLD автомат |

### 07. Опциональные фичи

| Документ | Фича |
|----------|------|
| [01-rfid.md](07-features/01-rfid.md) | События tag_detected / tag_removed, чтение/запись OpenPrintTag |
| [02-websocket-local.md](07-features/02-websocket-local.md) | Локальный WebSocket-канал без облака |
| [03-link-integrations-overview.md](07-features/03-link-integrations-overview.md) | **LINK-интеграции**: общий контракт HA / Bambu / Moonraker *(design-level)* |
| [04-home-assistant.md](07-features/04-home-assistant.md) | Home Assistant: публикация сенсоров через mDNS |
| [05-bambu-integration.md](07-features/05-bambu-integration.md) | Bambu Lab: apply филамента через LAN MQTT |
| [06-moonraker-printer.md](07-features/06-moonraker-printer.md) | Moonraker / Klipper: статус печати через WebSocket |
| [07-portal-integration-contract.md](07-features/07-portal-integration-contract.md) | **API-reference для разработчика портала** (все три интеграции) |

---

## Стандартные возможности всех устройств семейства

Независимо от `deviceType` (сушилка / нагреватель / телеметрия / другие будущие продукты):

- **Локальное меню MCU** — определено в `menu_meta.h`, общем для MCU и LINK.
- **Remote config** — те же элементы меню доступны удалённо через стандартные MQTT-команды:
  - `commands/get_config` — запросить полный JSON меню.
  - `commands/set` — изменить значение.
  - `commands/invoke` — выполнить action.
  - Публикации: `config` (полный снимок, retained) и `config/delta` (инкремент).
- **UI-следствие:** «шестерёнка» на плашке устройства в дашборде — стандартная функция для всех устройств, не специфична для типа.

Подробно: [06-flows/03-remote-config.md](06-flows/03-remote-config.md), [04-mqtt/04-backend-to-device.md](04-mqtt/04-backend-to-device.md).

---

## Быстрые ответы

- **Мне нужен свой контроллер совместимый с iDryer, с чего начать?** → [02-getting-started/02-quickstart-controller.md](02-getting-started/02-quickstart-controller.md).
- **Как посчитать CRC-16 для UART?** → [03-uart/02-frame-and-crc.md](03-uart/02-frame-and-crc.md) (готовая функция на C и Python).
- **Что лежит в `HelloPayload` по байтам?** → [03-uart/04-binary-structures.md#hellopayload-0x01--86-байт](03-uart/04-binary-structures.md).
- **Как опубликовать телеметрию в правильный топик?** → [04-mqtt/02-topics.md](04-mqtt/02-topics.md) и [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md).
- **Чем `unitId: 0` отличается от `"U1"`?** → [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md) (warning в разделе `info`) и [01-overview/04-glossary.md](01-overview/04-glossary.md).
- **Какой URL у API провижн-запроса?** → [05-cloud/02-http-api.md](05-cloud/02-http-api.md).
- **Устройство offline, что публикует брокер?** → `idryer/<serial>/offline` с телом `{}`. Настройка LWT — [04-mqtt/01-connection.md](04-mqtt/01-connection.md).

---

## Известные ограничения (release checklist)

Если вы делаете продукт на основе библиотеки — проверьте, какие из этих ограничений критичны для вашего сценария. Детали — в соответствующих разделах.

### ⚠️ Частично реализовано

| Ограничение | Где описано |
|-------------|-------------|
| Фрагментация исходящего `ConfigPush` из MQTT `set`/`invoke` — не реализована при JSON > 194 байт | [03-uart/03-message-types.md](03-uart/03-message-types.md), [06-flows/03-remote-config.md](06-flows/03-remote-config.md) |

### ❌ Не реализовано (только контракт)

| Ограничение | Где описано |
|-------------|-------------|
| Публикация `Log` (0x60) в MQTT `events` — не автоматическая, требует `setLogHandler()` в прикладном коде | [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md), [03-uart/04-binary-structures.md](03-uart/04-binary-structures.md) |
| **LINK-интеграции** (HA credentials, Bambu config+apply, Moonraker status) — контракт готов, LINK-сторона не реализована | [07-features/03-link-integrations-overview.md](07-features/03-link-integrations-overview.md), [07-features/07-portal-integration-contract.md](07-features/07-portal-integration-contract.md) |

### ℹ️ Дизайн-ограничения, важно знать

| Ограничение | Где описано |
|-------------|-------------|
| Референсный `UartBridge` не сбрасывает парсер по межбайтовому таймауту — только при ошибках CRC/size | [03-uart/05-ack-retry.md](03-uart/05-ack-retry.md) |
| `UartBridge` не ведёт дедупликацию по SEQ — повторно доставленные кадры обрабатываются дважды | [03-uart/05-ack-retry.md](03-uart/05-ack-retry.md) |
| Поле `Heartbeat.wifiRssiDbm` перегружено: MCU→LINK прошивка MCU может писать туда температуру × 10 | [03-uart/04-binary-structures.md](03-uart/04-binary-structures.md) |
| `unitId` несимметричен: в `info` — число 0–3, в остальных топиках — строка `"U1"`…`"U4"` | [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md) |
| `readerId` в MQTT остаётся **числом** 0–3 (в отличие от `unitId`/`sensorId` как строки) | [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md) |
| `startStage` в команде `profile` не валидируется LINK относительно `stages.length` — клиент проверяет сам | [04-mqtt/04-backend-to-device.md](04-mqtt/04-backend-to-device.md) |
| Общий таймаут claim «сколько ждать ввода PIN» не зафиксирован в библиотеке — задача прикладной прошивки | [05-cloud/01-claiming-overview.md](05-cloud/01-claiming-overview.md) |

---

## Источники правды

Документация сверена с кодом по следующим ключевым файлам. При расхождении между этой документацией и кодом — **побеждает код**:

- `src/uart/uart_protocol.h` — все UART-структуры, enum-ы, размеры.
- `src/uart/uart_protocol.cpp` — реализация CRC16.
- `src/uart/uart_bridge.cpp` — парсер, ACK/retry.
- `src/mqtt/idryer_topics.h` — константы топиков, QoS, retained.
- `src/mqtt/mqtt_client.cpp` — подключение, LWT, `publishInfo`.
- `src/cloud/telemetry_publisher.cpp` — JSON-форматы публикаций.
- `src/cloud/command_handler.cpp` — разбор MQTT-команд.
- `src/cloud/cloud_state_machine.cpp` — автомат состояний облака.
- `src/cloud/command_sink.h` / `uart_command_sink.h` — `ICommandSink`.

---

## Внешние документы

Эти материалы находятся **вне** этого репозитория, но могут пригодиться:

- **Репозиторий iDryer Link** — [github.com/pavluchenkor/idryer-link](https://github.com/pavluchenkor/idryer-link) — референсная потребительская прошивка на базе этой библиотеки.
- **Репозиторий iDryer Portal** — бэкенд с документацией claiming (`docs/development/DEVICE_CLAIMING_PROTOCOL.md`, `LINK_CLAIM_SCENARIOS.md`, `PROVISION_SECURITY_DESIGN.md`). Путь зависит от вашего доступа к инфраструктуре iDryer.
- **`BAMBU_EMULATION_PLAN.md`** — в корне репозитория `iDryerRP2040/`, исходный дизайн Bambu-интеграции (status портала на апрель 2026).
- **`iHeater-link/virtual_chamber_guide.md`** — проверенная инструкция для пользователя iHeater: настройка `[gcode_macro VIRTUAL_CHAMBER]` в Klipper для автоматического управления температурой камеры через `M141` в стартовом G-code слайсера.

---

## Архив

В `_legacy/` — старая структура документации, сохранена для сверки во время миграции. После стабилизации новой версии будет удалена.

---

## Обратная связь

Нашли расхождение между документацией и кодом, непонятную формулировку, ошибку в hex-дампе? Откройте issue в репозитории `idryer-protocol` или напишите разработчику.

**Автор библиотеки:** Ruslan Pavluchenko · `pavluchenko.r@gmail.com`
**Лицензия:** MIT
