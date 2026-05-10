# Опциональные фичи

Всё что **не обязательно** для базового устройства iDryer, но добавляет ценность. Две «родные» фичи (RFID-метки, локальный WebSocket без облака) и пять документов про LINK-интеграции с внешними системами (Home Assistant, Bambu Lab, Moonraker/Klipper).

| Документ | Фича |
|----------|------|
| [01-rfid.md](01-rfid.md) | RFID-ридеры: tag_detected / tag_removed, чтение/запись OpenPrintTag |
| [02-websocket-local.md](02-websocket-local.md) | Локальный WebSocket-канал LINK для работы без облака |
| [03-link-integrations-overview.md](03-link-integrations-overview.md) | **Общий контракт** трёх LINK-интеграций: один топик `link_integration`, сводный `integrations/status`, поведение по `deviceType` |
| [04-home-assistant.md](04-home-assistant.md) | Home Assistant: публикация сенсоров через mDNS + credentials из портала |
| [05-bambu-integration.md](05-bambu-integration.md) | Bambu Lab LAN MQTT: **Writer** (эмуляция RFID-метки для Dryer) / **Reader** (статус принтера для iHeater) |
| [06-moonraker-printer.md](06-moonraker-printer.md) | Moonraker (Klipper) WebSocket + **VIRTUAL_CHAMBER** macro для iHeater |
| [07-portal-integration-contract.md](07-portal-integration-contract.md) | **Контракт для портального разработчика**: все три интеграции в одном документе |

## Как читать

- **Если вы — портальный разработчик** → сразу [07-portal-integration-contract.md](07-portal-integration-contract.md). Там есть всё необходимое, дальше по желанию.
- **Если вы — разработчик прошивки LinkII (iHeater)** → [03-link-integrations-overview.md](03-link-integrations-overview.md) для общей архитектуры, затем [06-moonraker-printer.md](06-moonraker-printer.md) (это главный поставщик целевой температуры камеры).
- **Если вы делаете устройство iDryer с RFID** → [01-rfid.md](01-rfid.md).
- **Если хотите локальный доступ без интернета** → [02-websocket-local.md](02-websocket-local.md).

## Статус реализации

Большинство LINK-интеграций (Bambu / Moonraker / HA с credentials через портал) реализованы в библиотеке на `feat/link-integrations`→`dev`, см. commit-ы `Phase 1..4`. Детали по каждой — в соответствующих документах.
