# Контракт для разработчика портала: LINK-интеграции

Один документ. Всё, что нужно разработчику backend/frontend портала, чтобы реализовать передачу настроек трёх интеграций (Home Assistant, Bambu Lab, Moonraker) в LINK и отображать их статус в UI. Детальные обоснования и семантика — в отдельных документах, ссылки по ходу.

!!! note "Статус"
    LINK-сторона **не реализована**. Контракт зафиксирован — портал можно разрабатывать параллельно. Тестовый стенд LINK появится после реализации, до этого можно эмулировать ответы руками через `mosquitto_pub`.

---

## Стандартная функция: меню MCU + remote config

**Все устройства семейства iDryer** (сушилки, нагреватели, телеметрия — любой `deviceType`) обязаны иметь:

- **Локальное меню на экране MCU** с элементами настроек, описанными в `menu_meta.h`.
- **Remote config**: те же элементы меню доступны удалённо через стандартные команды MQTT — `commands/get_config`, `commands/set`, `commands/invoke`. Поток: `config` (retained, полный снимок) ↔ `config/delta` (изменения).

UI-следствие для портала: **«шестерёнка» на плашке устройства в дашборде** → модалка с редактированием всех элементов меню устройства. **Эта функция одинакова для iDryer, iHeater и любых будущих продуктов** — не специфична для LINK-интеграций.

Детали протокола remote config: [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md).

---

## Архитектура (одним абзацем)

Пользователь в UI портала нажимает кнопку **«Home Assistant»**, **«Bambu Lab»** или **«Moonraker»** → открывается модальное окно с полями → пользователь заполняет → нажимает **Save** → backend **публикует MQTT-сообщение** в устройство (credentials **не сохраняет** у себя) → LINK сохраняет в NVS → LINK публикует `integrations/status` → UI отображает текущее состояние. Активная интеграция выбирается отдельно (в меню MCU/LINK), в NVS могут быть все три.

## Ручное управление температурой камеры iHeater — существующий канал `commands/drying`

Для `deviceType: heater` / `link_ii` **плашка на дашборде «Set chamber temperature»** использует ровно тот же `commands/drying`, что у iDryer — **новой команды не нужно**.

```json
// idryer/<serial>/commands/drying
{
  "unitId": "U1",
  "params": {
    "temperature": 50,
    "duration": 0
  }
}
```

- `temperature` — целевая температура камеры в °C.
- `duration: 0` — бессрочно.
- `duration: N` — автоотключение через N минут.

UI портала для `deviceType: heater` может показывать лейбл «Температура камеры» вместо «Температура сушки», но тело JSON идентично.

### Три источника температуры камеры iHeater

Для MCU iHeater есть три независимых входа — протокол их разделяет, приоритет решает прошивка:

1. **`VIRTUAL_CHAMBER.target`** (Moonraker → LINK → UART) — автомат, привязан к печати.
2. **`commands/drying`** (портал → MQTT → LINK → UART `CommandPayload`) — ручной удалённый.
3. **Меню на экране iHeater** — локальный fallback.

Рекомендуемый приоритет в прошивке: Moonraker во время активной печати > drying > меню. Это НЕ контракт протокола — решает конкретная прошивка iHeater-MCU.

---

## ⚠️ Поведение зависит от `deviceType`

Одно и то же подключение (Bambu LAN MQTT или Moonraker WS) используется **по-разному** в зависимости от типа устройства:

| `deviceType` (из `info`) | Режим | Что делает LINK с Bambu-коннектом | Что делает LINK с Moonraker-коннектом |
|--------------------------|-------|------------------------------------|----------------------------------------|
| `"dryer"` / `Unknown (legacy)` | **Writer** | Шлёт `ams_filament_setting` по `bambu_apply` (эмуляция RFID-метки) | (MVP не используется) |
| `"heater"` / `"link_ii"` | **Reader** | Подписан на `device/<serial>/report`, читает филамент/статус → в MCU для управления нагревом камеры | Подписан на `gcode_macro VIRTUAL_CHAMBER.target` → передаёт значение MCU (0 = выкл, >0 = температура камеры) |

**Что это значит для backend-а портала:**

- **Публикуй `bambu_apply` ТОЛЬКО если** `info.deviceType in ["dryer", отсутствует (legacy)]`.
- **Для `heater` / `link_ii`** — не публикуй `bambu_apply` никогда. Им это не нужно и они это игнорируют.
- Настройки (`link_integration`) публикуются **одинаково** для всех типов — разница только в `bambu_apply`.

---

## Что делает backend

### 1. HTTP endpoint для фронтенда (один на все три)

```
POST /devices/:deviceId/link-integration
Authorization: Bearer <user JWT>
Content-Type: application/json
```

Тело — один из трёх payload-ов (см. раздел «Payload»). Backend:

1. Проверяет ownership: `deviceId` принадлежит пользователю из JWT.
2. Проверяет `isOnline`: `Link` в состоянии `BOUND` и онлайн в MQTT.
3. Публикует в MQTT: `idryer/<serial>/commands/link_integration` с тем же телом, QoS 1, retain false.
4. **Не сохраняет** `password`, `lanAccessCode`, `apiKey` в БД.
5. Возвращает `200 OK { "published": true }` или `409 Conflict` если устройство оффлайн.

Рекомендация: опционально хранить **non-secret мета** (был ли настроен HA; IP Bambu без lanAccessCode) для удобства UI — но только **масками и не-секретами**.

### 2. Обработка `integrations/status` от LINK

Backend подписан (в сервисе MQTT auth hook или в основном сервисе) на `idryer/+/integrations/status`. При получении:

- Распаковать JSON.
- Прокинуть на фронт через WebSocket/SSE, чтобы пользователь видел изменение онлайн.

Опционально кэшировать (последний известный) для отображения в списке устройств — это retained-топик, так что даже при offline устройстве фронт получит последнее состояние.

### 3. Bambu-specific: публикация `bambu_apply` при tag_detected

**Только для `deviceType == "dryer"` (или legacy/Unknown).** Для Heater/LinkII не публиковать.

Это отдельный поток, описанный в [05-bambu-integration.md](05-bambu-integration.md). Backend при получении `idryer/<serial>/rfid` с `event: tag_detected`:

1. Проверяет `deviceType` из сохранённого `info` устройства:
   - `"dryer"` или отсутствует → продолжить.
   - `"heater"` / `"link_ii"` / другое → **стоп**, не публиковать.
2. Ищет spool по `uid`.
3. Если найден и у spool есть filament profile: собирает Bambu-совместимый payload (см. ниже).
4. Публикует `idryer/<serial>/commands/bambu_apply`.

Важно: для `deviceType == "heater"` события `tag_detected` и не придут — у iHeater нет RFID-ридеров (в `info.units[].rfid` будут пустые массивы). Но для защиты от будущих изменений лучше явно проверять `deviceType`, а не полагаться только на наличие события.

---

## Что делает frontend

### Три кнопки в карточке устройства

- **Home Assistant** → модалка с полями `host`, `port`, `username`, `password`, `discoveryPrefix`.
- **Bambu Lab** → модалка с полями `ip`, `serial`, `lanAccessCode`, `defaultAmsId` (255), `defaultTrayId` (254), `autoApplyOnTagDetect` (true).
- **Moonraker** → модалка с полями `host`, `port` (7125), `apiKey` (optional), `ssl` (false), `pollIntervalMs` (1000).

Кнопки **disabled + tooltip** если Link не в `BOUND` или устройство оффлайн.

### Сохранение

При нажатии **Save** в любой модалке:

```
POST /devices/:deviceId/link-integration
{ "type": "ha"|"bambu"|"moonraker", "enabled": true, ... }
```

### Отображение статуса

Читать `integrations/status` (через backend WebSocket/SSE). Для каждой секции (`ha`, `bambu`, `moonraker`) показывать:

- `configured` → индикатор «настроено / нет».
- `state` → цветной badge (`online` = зелёный, `connecting` = жёлтый, `error`/`config_missing` = красный).
- `lastError` → tooltip с текстом ошибки.
- Специфика Bambu: `printerIp`, `printerSerial`, `printerState`, `progress`.
- Специфика Moonraker: `host`, `printerState`, `progress`, `filename`.

### Активная интеграция

В интерфейсе показывать, какая из трёх **активна сейчас** (из `integrations/status.active`). Сменить её — через меню устройства (поле `activeIntegration` в меню MCU). Это не отдельная кнопка модалки, это общий механизм remote config — см. [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md).

**Опционально** в MVP: можно добавить быстрый переключатель в UI портала, который шлёт `commands/set { "id": <menu-id-active>, "val": "ha"|"bambu"|"moonraker"|"none" }` напрямую.

---

## Payload: `commands/link_integration`

Публикуется в `idryer/<serial>/commands/link_integration`, QoS 1, retain false.

Дискриминатор — поле `type`. Частичный payload: портал публикует только одну секцию за раз, LINK мёржит в NVS поверх существующего.

### type = `"ha"`

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

Все поля кроме `type`, `enabled` — опциональные. Дефолты — см. [04-home-assistant.md](04-home-assistant.md).

### type = `"bambu"`

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

### type = `"moonraker"`

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

---

## Payload: `commands/bambu_apply`

Публикуется в `idryer/<serial>/commands/bambu_apply`, QoS 1, retain false. Отдельный поток — только для Bambu при tag_detected.

**Публиковать только для `deviceType == "dryer"` (или legacy/Unknown).** Для Heater/LinkII не шлём.

```json
{
  "amsId": null,
  "trayId": null,
  "trayType": "PLA",
  "colorHex": "FFAABBFF",
  "nozzleTempMin": 209,
  "nozzleTempMax": 231,
  "trayInfoIdx": "GFL99",
  "settingId": "",
  "spoolId": "uuid-abc-123",
  "uid": "AABB1234"
}
```

### Как backend заполняет поля

Источники (имена полей зависят от схемы БД портала):

Поле payload | Источник
---|---
`trayType` | `filamentType.code` (PLA / ABS / PETG / ...) |
`colorHex` | `filamentSpec.colorHex` или `materialSnapshot.colorHex`; 8 hex + FF или 6 hex |
`nozzleTempMin` | `printPreset.printNozzleTemp * 0.95` |
`nozzleTempMax` | `printPreset.printNozzleTemp * 1.05` |
`trayInfoIdx` | маппинг `(filamentType, brand)` → Bambu-код (`GFL99`, `GFA00`, ...); таблица в коде backend |
`settingId` | `""` |
`amsId` / `trayId` | `null` — LINK подставит дефолты из NVS |
`spoolId` | UUID spool'а (для диагностики) |
`uid` | UID метки из `tag_detected` |

### Таблица `trayInfoIdx`

Семейство Bambu-кодов (частичный список, для MVP достаточно):

Тип | Brand generic | Bambu Basic
---|---|---
PLA | `GFL99` | `GFA00`
PETG | `GFL98` | `GFG00`
ABS | `GFL97` | `GFB00`
TPU | `GFL96` | `GFU00`
ASA | `GFL95` | `GFB01`
PC | `GFL94` | `GFC00`
PA (nylon) | `GFL93` | `GFN03`

Полная таблица — в документации OpenSpool и `OpenBambuAPI`.

---

## Topic: `integrations/status` (LINK → portal)

```
Topic:    idryer/<serial>/integrations/status
QoS:      1
Retained: true
```

Полный снимок, публикуется при любом изменении любого поля любой из трёх секций.

```json
{
  "active": "moonraker",
  "ha": {
    "configured": true,
    "enabled": true,
    "state": "disabled",
    "host": "homeassistant.local",
    "brokerPort": 1883,
    "authUsed": true,
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:00Z"
  },
  "bambu": {
    "configured": true,
    "enabled": false,
    "state": "disabled",
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
    "port": 7125,
    "printerState": "printing",
    "progress": 42,
    "remainingSeconds": 3600,
    "filename": "benchy.gcode",
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:12Z"
  }
}
```

### Значения `state` (общие)

- `"disabled"` — `active != этот тип`, клиент выключен.
- `"idle"` — активен, настройки есть, ждёт работы.
- `"connecting"` — идёт попытка подключения.
- `"online"` — работает.
- `"config_missing"` — активен, но параметры невалидны.
- `"error"` — ошибка, см. `lastError`.

### Дополнительно для Bambu

Общие поля (есть в обоих режимах): `printerIp`, `printerSerial`, `printerState`, `progress`, `remainingSeconds`, `currentLayer`, `totalLayers`, `nozzleTemp`, `nozzleTarget`, `bedTemp`, `bedTarget`.

Только в **Writer-режиме** (`deviceType: dryer`): `lastApply { at, result, spoolId, amsId, trayId }`.

Только в **Reader-режиме** (`deviceType: heater / link_ii`): `currentFilament` (тип филамента текущей печати, считан с принтера), плюс специфичные поля управления камерой — они отдаются MCU iHeater, но могут и публиковаться в status для UI.

### Дополнительно для Moonraker

- `host`, `port`, `virtualChamberAvailable`, `chamberHasSensor`, `chamberTarget`, `chamberTemperature`, `printerState`, `progress`, `remainingSeconds`, `filename`, `currentLayer`, `totalLayers`, `nozzleTemp`, `nozzleTarget`, `bedTemp`, `bedTarget`, `printDurationSeconds`.

Ключевые поля для iHeater:

- **`chamberTarget`** (float, °C) — `VIRTUAL_CHAMBER.target` из Klipper (устанавливается через `M141 S<temp>` в слайсере). Setpoint нагрева.
- **`chamberTemperature`** (float, °C) — `VIRTUAL_CHAMBER.temperature`. Feedback для PID. Валидно только при `chamberHasSensor == true`.
- **`chamberHasSensor`** (bool) — есть ли реальный температурный датчик, пробрасывающий значение в macro. `false` → MCU iHeater работает по своему локальному датчику.
- **`virtualChamberAvailable`** (bool) — Klipper вернул объект `gcode_macro VIRTUAL_CHAMBER`. `false` → пользователю показать инструкцию по настройке (проверенный гайд — `/docs/iHeater-link/virtual_chamber_guide.md`).

### Периодичность публикации status

- При любом структурном изменении (active, configured, connection state, target, hasSensor, lastApply) — **мгновенно**.
- Дополнительно **раз в 30 секунд** — чтобы `chamberTemperature` и `progress` не устаревали в retained-snapshot. Таймер сбрасывается на любой явной публикации.
- Спама нет: между событиями не чаще 1 раз / 30 с.

### Значения `printerState`

Унифицированные для обоих (Bambu и Moonraker):

- `"idle"` — простой
- `"prepare"` — Bambu preheat / Klipper preparing
- `"printing"` — печатает
- `"paused"` — на паузе
- `"finished"` / `"complete"` — завершено
- `"cancelled"` — отменено (только Klipper)
- `"error"` — ошибка печати

---

## Tag-detected → bambu_apply: последовательность (только `deviceType: dryer`)

```
RP2040 → UART Rfid(tag_detected, uid=AABB1234) → LINK
LINK → MQTT idryer/<serial>/rfid { "event": "tag_detected", "uid": "AABB1234", ... }
Backend:
  1. Проверить: info.deviceType == "dryer" или отсутствует? Нет → стоп.
  2. Найти spool по uid.
  3. Собрать bambu_apply payload.
  4. Публикация → idryer/<serial>/commands/bambu_apply
     (всегда, если spool найден — backend не проверяет autoApplyOnTagDetect,
      этот флаг хранится в NVS LINK и только LINK его читает)
LINK (Dryer):
  - autoApplyOnTagDetect == false → молча игнорирует.
  - active != bambu или !configured → публикует status.state = "disabled" / "config_missing".
  - Иначе → подключается к принтеру, шлёт ams_filament_setting, публикует status.lastApply.
```

Backend публикует `bambu_apply` при любом найденном spool — решение о применении принимает LINK на основе своих настроек. Это позволяет менять `autoApplyOnTagDetect` на LINK без какой-либо синхронизации с сервером.

Для iHeater (`deviceType: "heater"` / `"link_ii"`) этот поток не запускается: у них нет RFID-ридеров и backend их фильтрует по `deviceType`.

---

## Безопасность

- Backend **никогда** не хранит `password` (HA), `lanAccessCode` (Bambu), `apiKey` (Moonraker) в БД.
- При необходимости отображать «поле было заполнено» — хранить только маркер `configured: true/false`, без значения. LINK сам возвращает маркеры в `integrations/status`.
- В логах backend маскировать эти поля (`****`).
- Rate limit на `POST /devices/:id/link-integration`: 10 / 60 c (аналог существующих `/provision`).

---

## Проверка из командной строки (до готовности LINK)

Эмулируем LINK вручную:

```bash
# Подписаться на команды (играть роль LINK)
mosquitto_sub -h BROKER -p 8883 --cafile ca.pem \
  -u $SERIAL -P $TOKEN \
  -t "idryer/$SERIAL/commands/link_integration" \
  -t "idryer/$SERIAL/commands/bambu_apply" -v

# Опубликовать ответ-статус (играть роль LINK)
mosquitto_pub -h BROKER -p 8883 --cafile ca.pem \
  -u $SERIAL -P $TOKEN -q 1 -r \
  -t "idryer/$SERIAL/integrations/status" \
  -m '{ "active":"none", "ha":{...}, "bambu":{...}, "moonraker":{...} }'
```

Фронтенд при этом видит status, показывает состояние. Можно отлаживать весь UI до готовности LINK.

---

## Чек-лист для портального разработчика

Backend:

- [ ] `POST /devices/:deviceId/link-integration` — endpoint с проверкой ownership + online. Одинаково для всех `deviceType`.
- [ ] Публикация в MQTT `idryer/<serial>/commands/link_integration` без сохранения секретов в БД.
- [ ] Подписка на `idryer/+/integrations/status`, маршрутизация на фронт.
- [ ] (Bambu) При получении `idryer/<serial>/rfid` с `tag_detected`:
  - [ ] Проверить `info.deviceType`. Если `heater` / `link_ii` / любое не-dryer — **стоп**.
  - [ ] Иначе: поиск spool, публикация `commands/bambu_apply`.
- [ ] (Bambu) Реализация маппинга `(type, brand) → trayInfoIdx` (таблица Bambu-кодов).
- [ ] Маскировка паролей/кодов/ключей в логах.
- [ ] Rate limit на новый endpoint.

Frontend:

- [ ] Три кнопки в DeviceShow: HA / Bambu / Moonraker, disabled если устройство оффлайн.
- [ ] Три модальных окна с полями (см. раздел «Payload»).
- [ ] POST при Save, обработка 200/409.
- [ ] Визуализация `integrations/status` (badge по `state`, tooltip с `lastError`).
- [ ] (Опционально) Быстрый переключатель активной интеграции через `commands/set`.

Тестовый сценарий:

- [ ] Открыть HA-модалку → Save → увидеть `state: "disabled"` (пока не активна) и `configured: true`.
- [ ] В меню устройства выбрать HA → `state: "online"` → в HA появляются сенсоры.
- [ ] То же для Bambu, Moonraker.
- [ ] Подать фейковый `tag_detected` → увидеть `bambu_apply` в MQTT и `lastApply: "ok"` в status.

---

## Ссылки

- Общий контракт и принципы: [03-link-integrations-overview.md](03-link-integrations-overview.md).
- HA-специфика: [04-home-assistant.md](04-home-assistant.md).
- Bambu-специфика: [05-bambu-integration.md](05-bambu-integration.md).
- Moonraker-специфика: [06-moonraker-printer.md](06-moonraker-printer.md).
- Архив дизайн-плана Bambu (исходная постановка): `../../../BAMBU_EMULATION_PLAN.md` в корне репозитория iDryerRP2040.
