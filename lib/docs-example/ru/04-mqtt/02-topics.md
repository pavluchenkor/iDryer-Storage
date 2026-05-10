# MQTT: топики

Полный список топиков iDryer. Префикс для всех — `idryer/<serialNumber>/`.

!!! note "Источник правды"
    `src/mqtt/idryer_topics.h` — константы имён, QoS, retained, интервалов.

---

## Префикс

```
idryer/<serialNumber>/<suffix>
```

Пример: для устройства с `serialNumber = DEVICE_aabbccddeeff_1234567` топик телеметрии — `idryer/DEVICE_aabbccddeeff_1234567/telemetry`.

---

## Устройство → Backend

| Суффикс | QoS | Retained | Интервал | Назначение |
|---------|-----|----------|----------|------------|
| `info` | 1 | **да** | однократно | Статическая информация: версии, units, capabilities, mcuSerial |
| `telemetry` | 0 | нет | ~5 с | Температура, влажность, heater, fan |
| `status` | 1 | **да** | по изменению | Режим работы, таймеры, сессия |
| `weights` | 1 | нет | ~10 с / по событию | Веса филамента |
| `rfid` | 1 | **да** | по событию | tag_detected / tag_removed, также данные метки |
| `events` | 1 | нет | по событию | Логи приложения и ошибки |
| `config` | 1 | нет | по запросу | Полный JSON меню (~3 КБ) |
| `config/delta` | 1 | нет | по изменению | Delta-обновление меню |
| `offline` | 1 | нет | — | LWT брокера при аварийном отключении |
| `integrations/status` | 1 | **да** | при изменении | Статус LINK-интеграций (HA/Bambu/Moonraker). *Design-level, не реализовано.* |

**Интервалы** указаны как ориентиры. Фактическая частота задаётся прикладным кодом:

- UART от MCU приходит с частотой 1 с (активный) / 15 с (idle).
- В MQTT константа `IDRYER_INTERVAL_TELEMETRY_MS = 5000` — рекомендация, как часто продублировать последнее в брокер.

### Retained: что видит новый клиент

`info`, `status`, `rfid` — retained. Новое приложение, подписавшись на эти топики, сразу получает последние сообщения, даже если устройство в этот момент не публикует. Это позволяет UI открыть страницу устройства без задержки.

`telemetry`, `weights`, `events`, `config`, `config/delta` — не retained. Новый клиент ждёт следующей публикации.

---

## Backend → Устройство

Все команды идут в подписку `idryer/<serial>/commands/#`. Суффикс — имя команды.

| Топик | QoS | Назначение |
|-------|-----|------------|
| `commands/drying` | 1 | Запуск обычной сушки |
| `commands/storage` | 1 | Запуск режима хранения |
| `commands/profile` | 1 | Запуск профильной сушки |
| `commands/stop` | 1 | Остановка |
| `commands/find` | 1 | Поиск устройства (мигание) |
| `commands/get_config` | 1 | Запрос полного JSON меню |
| `commands/set` | 1 | Изменить параметр меню |
| `commands/invoke` | 1 | Вызвать action из меню |
| `commands/read_rfid` | 1 | Запустить чтение RFID-метки |
| `commands/write_rfid` | 1 | Запустить запись RFID-метки (stop-and-wait ACK flow control) |
| `commands/clear_errors` | 1 | Сбросить лог ошибок EEPROM |
| `commands/ping` | 1 | Проверка «живости» (только лог) |
| `commands/link_integration` | 1 | Настройки HA/Bambu/Moonraker (*design-level, не реализовано*) |
| `commands/bambu_apply` | 1 | Применить филамент в Bambu при tag_detected (*design-level*) |

Формат JSON каждой команды — [04-backend-to-device.md](04-backend-to-device.md).

---

## Статический справочник (C-константы)

```c
// Из src/mqtt/idryer_topics.h
#define IDRYER_TOPIC_PREFIX          "idryer"
#define IDRYER_TOPIC_INFO            "info"
#define IDRYER_TOPIC_TELEMETRY       "telemetry"
#define IDRYER_TOPIC_STATUS          "status"
#define IDRYER_TOPIC_WEIGHTS         "weights"
#define IDRYER_TOPIC_RFID            "rfid"
#define IDRYER_TOPIC_EVENTS          "events"
#define IDRYER_TOPIC_CONFIG          "config"
#define IDRYER_TOPIC_CONFIG_DELTA    "config/delta"
#define IDRYER_TOPIC_OFFLINE         "offline"

#define IDRYER_TOPIC_CMD_DRYING      "commands/drying"
#define IDRYER_TOPIC_CMD_STOP        "commands/stop"
#define IDRYER_TOPIC_CMD_STORAGE     "commands/storage"
#define IDRYER_TOPIC_CMD_FIND        "commands/find"
#define IDRYER_TOPIC_CMD_GET_CONFIG  "commands/get_config"
#define IDRYER_TOPIC_CMD_SET         "commands/set"
#define IDRYER_TOPIC_CMD_INVOKE      "commands/invoke"
#define IDRYER_TOPIC_CMD_READ_RFID   "commands/read_rfid"
#define IDRYER_TOPIC_CMD_WILDCARD    "commands/#"
```

!!! note "Команды без отдельных констант"
    `profile`, `write_rfid`, `clear_errors`, `ping` — их константы в заголовке отсутствуют, но `CommandHandler::handleMqttCommand` распознаёт эти имена (последний сегмент топика). Используйте явные строки при сборке топика или добавьте свои `#define` в прикладном коде.

---

## Home Assistant (отдельный префикс)

Если собираетесь интегрировать устройство с Home Assistant, библиотека предоставляет отдельный слой (`HaMqttClient`, `HaPublisher`) с префиксом `homeassistant/...`. Это независимый от `idryer/...` контур. В этой документации он не рассматривается; источники — `src/mqtt/ha_mqtt_client.*`, `src/cloud/ha_publisher.*`.

---

## Что дальше

- [03-device-to-backend.md](03-device-to-backend.md) — форматы JSON публикаций.
- [04-backend-to-device.md](04-backend-to-device.md) — форматы JSON команд.
