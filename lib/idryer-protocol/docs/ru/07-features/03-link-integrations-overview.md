# LINK-интеграции: общий контракт

LINK может работать с внешними сервисами дополнительно к основному облаку iDryer:

- **Home Assistant** — публикация сенсоров устройства в свой HA-сервер пользователя.
- **Bambu Lab** — применение настроек филамента в принтер Bambu через LAN MQTT + чтение статуса принтера.
- **Moonraker (Klipper)** — чтение статуса принтера по WebSocket.

Все три построены по одной архитектурной модели. Эта страница описывает **общий контракт**. Детали каждой интеграции — в отдельных документах:

- [04-home-assistant.md](04-home-assistant.md)
- [05-bambu-integration.md](05-bambu-integration.md)
- [06-moonraker-printer.md](06-moonraker-printer.md)

!!! warning "Статус"
    **Design-level документация.** На стороне библиотеки `idryer-protocol` ни `commands/link_integration`, ни обработчиков для этих трёх интеграций **не реализовано**. На портале частично есть поддержка Bambu (endpoint `/configure-bambu`). Документы фиксируют целевой контракт до начала реализации.

---

!!! note "Меню и remote config — стандартны для всех устройств"
    Любое устройство семейства iDryer (сушилка, нагреватель, модуль телеметрии) имеет локальное меню на экране MCU и поддерживает удалённую настройку этих элементов через стандартные `commands/get_config` / `commands/set` / `commands/invoke`. «Шестерёнка» на плашке устройства в портале — стандартная UI-фича, одинаковая для всех. Документ ниже описывает **только** контракт внешних LINK-интеграций (HA/Bambu/Moonraker), которые живут **отдельно** от меню MCU. Не путать.

---

## Принципы

### 1. Credentials живут только в LINK

Портал **не хранит** `password`, `lanAccessCode`, `apiKey`. Пользователь в UI портала вводит их в модальном окне, нажимает «Save», фронтенд **напрямую публикует** в MQTT-топик команды, LINK принимает, сохраняет в NVS. Портал — только транспорт.

Это снижает риск компрометации: утечка БД портала не даёт доступа к принтерам и HA пользователя.

### 2. Один MQTT-топик команд на все три

```
idryer/<serial>/commands/link_integration
```

Payload — дискриминированный union по полю `type`. При каждом сохранении в модалке портал публикует payload только своей интеграции. LINK мёржит в NVS поверх существующего.

### 3. Одна активная за раз

В NVS хранятся все три набора параметров. Но LINK **поднимает клиента только для активной интеграции** — выбор делает пользователь в меню.

### 4. Один коннект — два режима в зависимости от `deviceType`

Подключение к Bambu/Moonraker одно, но что LINK делает с потоком данных — зависит от типа устройства (`HelloPayload.deviceType`, в MQTT `info.deviceType`):

| `deviceType` | Режим | Bambu | Moonraker |
|--------------|-------|-------|-----------|
| `Dryer` (0x01) / `Unknown` (legacy) | **Writer** — эмуляция RFID-метки | LINK **пишет** в принтер: `ams_filament_setting` по `bambu_apply` | (MVP не используется) |
| `Heater` (0x02) / `LinkII` (0x05) | **Reader** — чтение статуса для управления нагревом камеры | LINK **читает** `device/<serial>/report` → отдаёт MCU (тип филамента + статус печати; MCU решает температуру камеры по локальной таблице) | LINK **читает** `gcode_macro VIRTUAL_CHAMBER` (`target` + `temperature` + `has_sensor`) → отдаёт MCU; target = setpoint, temperature = feedback для PID (если hasSensor) |

Контракт конфигурации (`commands/link_integration`) — **одинаковый** для обоих режимов. Разница в том, что:

- Backend портала шлёт `commands/bambu_apply` **только для Dryer**. Для Heater/LinkII — не шлёт.
- LINK-прошивка после получения конфигурации поднимает либо **writer**, либо **reader** — по своему `deviceType`.

Это позволяет одной сушилке (iDryer) и одному нагревателю (iHeater) пользоваться одним и тем же Bambu LAN-подключением из разных точек комнаты, для разных задач, с одними и теми же credentials в разных NVS.

В конфигурации MCU+LINK: меню на MCU, поле `activeIntegration` приходит в LINK через обычный `ConfigPush` (элемент меню как любой другой).

В конфигурации standalone (LINK без MCU): меню на самом LINK, значение хранится локально.

Значения:

| `activeIntegration` | Что делает LINK |
|---------------------|-----------------|
| `"none"` | Все клиенты выключены (дефолт) |
| `"ha"` | Поднимает `HaMqttClient`, публикует discovery + state |
| `"bambu"` | Поднимает `BambuClient`, подключается к принтеру по LAN MQTT |
| `"moonraker"` | Поднимает WebSocket-клиент к Klipper-хосту |

Сменить активную интеграцию в рантайме разрешено. LINK должен корректно закрыть текущее соединение и открыть новое.

### 4. Общий status-топик обратно

```
idryer/<serial>/integrations/status
```

LINK публикует полный снимок состояния всех интеграций (retained, QoS 1). При изменении любого поля — publish заново. Фронтенд по этой публикации рисует статус для всех трёх модалок одновременно.

---

## Формат `commands/link_integration`

### HA

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

Поле | Обязательно | Примечание
---|---|---
`type` | да | `"ha"`
`enabled` | да | true = интеграция разрешена (но активируется она только если её выбрали в меню)
`host` | нет, дефолт `homeassistant.local` | mDNS-имя или IP
`port` | нет, дефолт `1883` | TLS-вариант не поддерживаем в MVP
`username` | нет | пусто = анонимный брокер
`password` | нет | вместе с `username`
`discoveryPrefix` | нет, дефолт `"homeassistant"` | сменить если HA настроен нестандартно

### Bambu

```json
{
  "type": "bambu",
  "enabled": true,
  "ip": "192.168.1.50",
  "serial": "039D09C40012345",
  "lanAccessCode": "12345678",
  "defaultAmsId": 255,
  "defaultTrayId": 254,
  "autoApplyOnTagDetect": true
}
```

Поле | Обязательно | Примечание
---|---|---
`type` | да | `"bambu"`
`enabled` | да |
`ip` | да | IP принтера в локальной сети
`serial` | да | serial number принтера, печатается на корпусе
`lanAccessCode` | да | 8-значный код из меню принтера (Settings → LAN Mode)
`defaultAmsId` | нет, дефолт `255` | AMS-слот по умолчанию (`255` = текущий активный)
`defaultTrayId` | нет, дефолт `254` | Tray по умолчанию (`254` = без AMS)
`autoApplyOnTagDetect` | нет, дефолт `true` | автоприменять при `tag_detected`

### Moonraker

```json
{
  "type": "moonraker",
  "enabled": true,
  "host": "klipper.local",
  "port": 7125,
  "apiKey": null,
  "ssl": false,
  "pollIntervalMs": 1000
}
```

Поле | Обязательно | Примечание
---|---|---
`type` | да | `"moonraker"`
`enabled` | да |
`host` | да | mDNS-имя или IP Klipper-хоста
`port` | нет, дефолт `7125` | стандарт Moonraker
`apiKey` | нет | `null` = open access; если Moonraker настроен на `[authorization]` с `force_logins` — нужен ключ
`ssl` | нет, дефолт `false` | `true` → `wss://`, иначе `ws://`
`pollIntervalMs` | нет, дефолт `1000` | интервал опроса статуса

---

## Формат `integrations/status`

```
Topic:    idryer/<serial>/integrations/status
QoS:      1
Retained: true
```

```json
{
  "active": "moonraker",
  "ha": {
    "configured": true,
    "enabled": true,
    "state": "idle",
    "host": "homeassistant.local",
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:00Z"
  },
  "bambu": {
    "configured": true,
    "enabled": false,
    "state": "idle",
    "printerIp": "192.168.1.50",
    "printerSerial": "039D09C40012345",
    "lastError": "",
    "updatedAt": "2026-04-19T11:55:00Z"
  },
  "moonraker": {
    "configured": true,
    "enabled": true,
    "state": "online",
    "host": "klipper.local",
    "printerState": "printing",
    "progress": 42,
    "remainingSeconds": 3600,
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:12Z"
  }
}
```

### Общие поля каждой секции

Поле | Тип | Описание
---|---|---
`configured` | bool | В NVS есть непустой валидный набор параметров
`enabled` | bool | `enabled: true` из последнего `commands/link_integration`
`state` | string | См. ниже
`lastError` | string | Человекочитаемая ошибка последней операции, пусто = OK
`updatedAt` | ISO 8601 | Момент последнего изменения статуса

### Значения `state`

Общие для всех интеграций:

- `"disabled"` — `active != этот тип`, клиент выключен
- `"idle"` — активен, настройки есть, ждёт работы
- `"connecting"` — идёт попытка подключения
- `"online"` — подключён и работает
- `"config_missing"` — активна, но параметры пустые или невалидные
- `"error"` — ошибка; см. `lastError`

Специфичные поля для Bambu: `printerIp`, `printerSerial`, `printerState`, `progress`, `remainingSeconds`, `lastApply` (результат последнего `bambu_apply`).

Специфичные поля для Moonraker: `host`, `printerState`, `progress`, `remainingSeconds`, `filename`, `currentLayer`, `totalLayers`.

Детали — в документах по каждой интеграции.

---

## Жизненный цикл (все три одинаковые)

```
1. Пользователь в UI портала открывает модалку "Bambu" → вводит IP/serial/code → Save.
2. Frontend публикует в MQTT:
      idryer/<serial>/commands/link_integration
      { "type": "bambu", "enabled": true, "ip": ..., ... }
3. LINK получает, валидирует, сохраняет секцию bambu в NVS.
4. LINK публикует integrations/status с обновлённой секцией bambu.
5. Пока active != "bambu" — LINK не поднимает BambuClient, но знает что он сконфигурирован.
6. Пользователь (локально на экране MCU или в меню LINK) меняет active → "bambu".
7. Изменение уходит в LINK через ConfigPush (или локально в standalone).
8. LINK закрывает текущего клиента (если был), поднимает BambuClient с сохранёнными параметрами.
9. LINK публикует integrations/status с active="bambu" и state="connecting" → "online".
```

---

## Как задаётся `active` в меню

### Конфигурация MCU+LINK

Новый элемент меню MCU, зарегистрированный в `menu_meta.h`:

- `id` — выделенный (например, `100`).
- `type` — enum с вариантами `none/ha/bambu/moonraker`.
- Значение хранится в EEPROM MCU как любой другой параметр меню.

Пользователь меняет локально (энкодер/экран) или удалённо:

```
MQTT commands/set { "id": 100, "val": "moonraker" }
  → LINK шлёт ConfigPush с {"cmd":"set","id":100,"val":"moonraker"} в MCU
  → MCU применяет, инкрементирует rev, сохраняет
  → MCU шлёт ConfigPush delta {"d":{"100":"moonraker"}} в LINK
  → LINK видит изменение → переключает клиент
```

Этот поток ничем не отличается от любого другого элемента меню (см. [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md)).

### Standalone LINK

Меню живёт на самом LINK. Значение `activeIntegration` хранится в NVS LINK вместе с параметрами интеграций. Изменение — через отдельный MQTT-канал или через локальный UI LINK (WebSocket, кнопки и т.п.).

---

## Безопасность

| Риск | Митигация |
|------|-----------|
| Утечка БД портала | Credentials не хранятся на портале |
| Перехват MQTT-команды | TLS между устройством и брокером iDryer |
| Компрометация брокера iDryer | Сильнее этого в модели угроз нет — брокер под контролем iDryer |
| Кто-то ещё публикует в `commands/link_integration` | EMQX проверяет `(serialNumber, deviceToken)`; публиковать может только владелец устройства (через портал, который проверяет ownership перед тем, как публиковать) |
| Логи LINK выдают пароль | Маскировать все `password`/`lanAccessCode`/`apiKey` в логах (выводить `****`) |

---

## Что дальше

- [04-home-assistant.md](04-home-assistant.md) — детали HA-интеграции (publisher + mDNS + discovery).
- [05-bambu-integration.md](05-bambu-integration.md) — Bambu: config + apply при tag_detected + status.
- [06-moonraker-printer.md](06-moonraker-printer.md) — Moonraker: WebSocket JSON-RPC, status printer.
