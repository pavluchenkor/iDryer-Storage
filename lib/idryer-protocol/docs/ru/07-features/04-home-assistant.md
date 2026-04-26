# Home Assistant: интеграция публикации сенсоров

LINK публикует сенсоры устройства (температура, влажность, режим, веса) в MQTT-брокер пользовательского Home Assistant через механизм discovery. После этого в HA автоматически появляются entity для каждой камеры и датчика.

!!! note "Статус"
    - **Публикатор (`HaMqttClient` + `HaPublisher`) существует** в коде библиотеки, работает через mDNS при дефолтных настройках.
    - **Передача параметров HA от портала в LINK** (`commands/link_integration` с `type: "ha"`) — **design-level**, не реализовано. Документ фиксирует целевой контракт.
    - **Авторизация (username/password)** и нестандартный `host`/`port` — **не проверено** в текущей прошивке; после реализации `commands/link_integration` эта цепочка должна работать.

!!! warning "Предполагается знакомство с Home Assistant"
    Документ не объясняет что такое HA, Mosquitto Add-on, discovery, entities. Если вы этого не знаете — сначала [официальный HA-docs об MQTT integration](https://www.home-assistant.io/integrations/mqtt/) и [MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery). Дальше вернитесь сюда.

---

## Роль

LINK — **MQTT-клиент пользовательского HA**. Пользователь не меняет свою установку HA — LINK сам появляется как устройство и публикует discovery.

```
   iDryer LINK                    Home Assistant
  ┌──────────────┐               ┌────────────────┐
  │ HaMqttClient ├──MQTT──────►│ mosquitto      │
  │              │               │                │
  │ HaPublisher  │               │ HA core        │
  └──────────────┘               └────────────────┘
                                          │
                                          ▼
                                     ┌─────────┐
                                     │ iDryer  │ ← появляется как device
                                     │ U1 Temp │ ← sensor entity
                                     │ U1 Hum  │
                                     │ U1 Mode │
                                     └─────────┘
```

---

## Конфигурация

### Дефолтный режим (mDNS, без credentials)

LINK запрашивает `homeassistant.local` через mDNS, подключается к порту `1883` без авторизации. Работает «из коробки» у пользователей, у которых HA доступен по этому имени и MQTT-брокер открыт анонимно.

Если `active = "ha"` и секция `ha` в NVS пустая — LINK использует дефолт.

### Через `commands/link_integration`

```json
{
  "type": "ha",
  "enabled": true,
  "host": "homeassistant.local",
  "port": 1883,
  "username": "mqtt_user",
  "password": "secret",
  "discoveryPrefix": "homeassistant"
}
```

Параметр | Дефолт | Примечание
---|---|---
`host` | `homeassistant.local` | mDNS или IP |
`port` | `1883` |  |
`username` | пусто | при заполнении используется MQTT auth |
`password` | пусто | в паре с `username` |
`discoveryPrefix` | `"homeassistant"` | сменить, если HA настроен на другой префикс |

Общий контракт (один топик на все интеграции, как пользовательские credentials не попадают на портал) — [03-link-integrations-overview.md](03-link-integrations-overview.md).

---

## Публикация в HA

### Discovery

При подключении LINK публикует discovery-сообщения в `homeassistant/<component>/<object_id>/config`:

- `homeassistant/sensor/idryer_<serial>_u1_temperature/config`
- `homeassistant/sensor/idryer_<serial>_u1_humidity/config`
- `homeassistant/sensor/idryer_<serial>_u1_heater_power/config`
- `homeassistant/binary_sensor/idryer_<serial>_u1_fan/config`
- ... для каждой камеры

Discovery payload содержит:

- `name` — `"iDryer U1 Temperature"`.
- `state_topic` — `idryer/<serial>/ha/u1/temperature`.
- `device_class` — `temperature`, `humidity`, `power_factor` и т.п.
- `unit_of_measurement` — `°C`, `%`, `%`.
- `device` — группа: `{ identifiers: [serial], manufacturer: "iDryer", model: "Dryer", sw_version }`.

### State

LINK публикует значения в отдельные state-топики (QoS 0, retained) с тем же интервалом, что и MQTT-телеметрия в iDryer-облако. Типично:

- `idryer/<serial>/ha/u1/temperature` → `"55.3"`
- `idryer/<serial>/ha/u1/humidity` → `"45.2"`
- `idryer/<serial>/ha/u1/heater_power` → `"80"`
- `idryer/<serial>/ha/u1/fan` → `"ON"` / `"OFF"`

### Availability (planned)

LWT на HA-брокер: `idryer/<serial>/ha/status` с payload `"offline"`. При подключении LINK публикует `"online"` (retained). В discovery каждого сенсора указать `availability_topic`.

---

## Текущая реализация (библиотека)

`HaMqttClient` в `src/mqtt/ha_mqtt_client.*`:

- Конструктор принимает необязательный `host`.
- `begin()` ищет HA через mDNS, если host не задан.
- `publish(topic, payload, retained)` — низкоуровневая публикация.

`HaPublisher` в `src/cloud/ha_publisher.*`:

- Принимает `HaMqttClient*`.
- Публикует discovery и state по сенсорам устройства.

**Что не реализовано:**

- Обработка `commands/link_integration` для сохранения параметров в NVS и переконфигурации `HaMqttClient`.
- Проверенной работы с `username`/`password` на HA с включённой авторизацией.
- LWT/availability.
- Реакция на изменение `active` в меню: поднятие/опускание `HaMqttClient`.

---

## `integrations/status` для HA

Секция `ha` в общем status-объекте ([03-link-integrations-overview.md](03-link-integrations-overview.md)):

```json
"ha": {
  "configured": true,
  "enabled": true,
  "state": "online",
  "host": "homeassistant.local",
  "brokerPort": 1883,
  "authUsed": true,
  "lastError": "",
  "updatedAt": "2026-04-19T12:00:00Z"
}
```

Дополнительные поля:

- `brokerPort` — фактический порт подключения.
- `authUsed` — `true` если были заданы `username`+`password`, иначе `false` (анонимный режим).

Значение `password` никогда не публикуется в status.

---

## Что дальше

- [03-link-integrations-overview.md](03-link-integrations-overview.md) — общий контракт команды и статуса.
- [05-bambu-integration.md](05-bambu-integration.md) — Bambu-интеграция (следующая по сложности).
- [06-moonraker-printer.md](06-moonraker-printer.md) — Moonraker/Klipper.
