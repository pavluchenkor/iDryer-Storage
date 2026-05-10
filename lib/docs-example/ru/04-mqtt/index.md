# MQTT

Контракт взаимодействия LINK с облаком iDryer: подключение к брокеру, топики, JSON-форматы публикаций и команд, примеры для ручной отладки.

| Документ | Содержание |
|----------|------------|
| [01-connection.md](01-connection.md) | Подключение: TLS/CA, credentials, LWT, keepalive, **persistent session (clean_session=false)** |
| [02-topics.md](02-topics.md) | Полная таблица топиков: device→backend и backend→device, QoS, retained |
| [03-device-to-backend.md](03-device-to-backend.md) | JSON-форматы всех публикаций устройства (info, telemetry, status, weights, rfid, events, config, integrations/status) |
| [04-backend-to-device.md](04-backend-to-device.md) | JSON-форматы всех команд от портала (drying, stop, profile, set, invoke, link_integration, …) |
| [05-examples.md](05-examples.md) | Готовые `mosquitto_pub`/`mosquitto_sub` для каждой команды |

## Навигация

- Настраиваете своё подключение с нуля → [01-connection.md](01-connection.md).
- Смотрите что шлёт устройство → [03-device-to-backend.md](03-device-to-backend.md).
- Смотрите что принимает устройство → [04-backend-to-device.md](04-backend-to-device.md).
- Отлаживаете руками с сервера → [05-examples.md](05-examples.md).

## Связанное

- [../03-uart/](../03-uart/index.md) — что приходит в LINK по UART (что затем превращается в MQTT-публикации).
- [../05-cloud/](../05-cloud/index.md) — HTTP-часть: как LINK получает `deviceToken` до подключения к MQTT.
- [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md) — сквозной сценарий `set`/`invoke`/`config/delta`.
