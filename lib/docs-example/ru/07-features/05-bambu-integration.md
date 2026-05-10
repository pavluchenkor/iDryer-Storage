# Bambu Lab: интеграция через LAN MQTT

LINK подключается к принтеру Bambu Lab в локальной сети через LAN MQTT. Цель: автоматически применять настройки филамента в AMS/Tray при обнаружении RFID-метки, и отображать статус принтера в приложении iDryer.

Архитектурное решение (из `BAMBU_EMULATION_PLAN.md`): **не эмулировать родную Bambu RFID по железу**, а отправлять `ams_filament_setting` в принтер по LAN MQTT, как делает [OpenSpool](https://github.com/spuder/OpenSpool). Сопряжение с Bambu ограничено сетевой интеграцией на LINK.

## Два режима — по `deviceType`

Одно и то же подключение (LAN MQTT) используется по-разному в зависимости от устройства:

| `deviceType` (из `info`) | Режим | Что LINK делает |
|--------------------------|-------|------------------|
| `Dryer` (0x01) или legacy | **Writer** — эмуляция RFID-метки | Пишет в принтер `ams_filament_setting` по команде `bambu_apply` от портала |
| `Heater` (0x02) / `LinkII` (0x05) | **Reader** — чтение статуса | Подписан на `device/<serial>/report`, читает тип филамента и статус печати, отдаёт MCU для управления нагревом камеры |

Контракт credentials (`commands/link_integration`) — одинаковый. Разница — в consumer-логике после подключения. Детали по каждому режиму ниже.

!!! note "Статус"
    - **Портал (backend + frontend)** — реализовано: `POST /devices/:id/configure-bambu`, UI-диалог в DeviceShow.
    - **LINK** — **не реализовано**: нет `BambuClient`, handler-ов `commands/bambu_config`, `commands/bambu_apply`, publish `integrations/status`/`bambu/status`.
    - Документ — design-level контракт между порталом и LINK.

!!! warning "Предполагается знакомство с экосистемой Bambu Lab"
    Документ оперирует терминами Bambu: AMS (Auto Material System — мультифиламентный модуль), Tray (слот для катушки в AMS), LAN Mode / LAN Access Code (режим локального доступа и его 8-значный код), `ams_filament_setting` (внутренняя команда Bambu MQTT), OpenSpool (открытый стандарт RFID для катушек). Если не работали с Bambu X1C/P1S — сначала [OpenSpool docs](https://openspool.io/) и [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI).

---

## Writer-режим (iDryer): end-to-end

Действует, когда `deviceType == "dryer"` (или legacy/Unknown).

Доступны два сценария применения — автоматический и ручной. Оба используют один и тот же LINK-путь и одинаковый payload для принтера; разница только в инициаторе.

### Сценарий А: автоматический (по детекту RFID)

```
1. Пользователь помещает катушку в сушилку. PN5180 (RFID-ридер на плате RP2040)
   детектит метку, передаёт uid по UART на LINK.
2. LINK публикует событие в облако:
     топик: idryer/<serial>/rfid
     данные: { "event": "tag_detected", "uid": "AABB1234", "unitId": 0 }
3. Backend находит катушку в базе по uid → собирает payload с параметрами
   материала (тип, цвет, температуры, Bambu-код trayInfoIdx).
4. Backend публикует bambu_apply — ВСЕГДА, независимо от autoApplyOnTagDetect.
   Портал не знает этот флаг и знать не должен: он хранится в памяти LINK.
     топик: idryer/<serial>/commands/bambu_apply
5. LINK получает bambu_apply и проверяет свои настройки из NVS:
   - autoApplyOnTagDetect == false → молча игнорирует, ничего не отправляет.
   - Bambu не configured (нет ip/serial/accessCode) → публикует
     integrations/status { state: "config_missing" }, не применяет.
   - Всё ок → строит ams_filament_setting, подключается к принтеру по MQTT,
     отправляет команду.
6. LINK публикует результат:
     топик: idryer/<serial>/integrations/status
     данные: { "bambu": { "state":"online", "lastApply": { "result":"ok", ... } } }
7. Принтер обновляет параметры катушки в слоте AMS/Tray.
```

!!! note "Почему autoApplyOnTagDetect проверяет LINK, а не портал"
    Флаг хранится в NVS LINK — портал его не знает и не хранит. Это осознанное решение: пользователь изменил флаг → SAVE → LINK запомнил → работает сразу, без синхронизации с сервером. Портал всегда шлёт `bambu_apply` при найденной катушке, LINK сам решает применять или нет.

### Сценарий Б: ручной (пользователь нажимает кнопку на портале)

Используется когда пользователь хочет явно применить катушку — независимо от флага `autoApplyOnTagDetect`.

```
1. Пользователь видит в портале детектированную катушку (или выбирает из списка).
2. Нажимает кнопку «Применить в принтер».
3. Backend публикует bambu_test — ручной apply.
   Payload — тот же формат, что у bambu_apply.
     топик: idryer/<serial>/commands/bambu_test
4. LINK получает bambu_test:
   - Игнорирует autoApplyOnTagDetect (ручной вызов всегда выполняется).
   - Проверяет только: Bambu configured AND enabled?
   - Если нет → публикует status { state: "config_missing" }.
   - Если да → строит ams_filament_setting, отправляет принтеру.
5. LINK публикует idryer/<serial>/integrations/status с результатом.
6. Принтер обновляет параметры катушки.
```

!!! note "Расширяемость: другие принтеры"
    Описанный LINK-путь одинаков для всех принтеров: портал всегда шлёт `bambu_apply` / `bambu_test` с одним и тем же payload — LINK смотрит какая интеграция активна и переводит на нужный протокол. Для Bambu — MQTT `ams_filament_setting`. Для Klipper/Moonraker — Moonraker REST API с G-code макросом. Backend, портал и топики не меняются. Детали: [06-moonraker-printer.md](06-moonraker-printer.md).

## Reader-режим (iHeater): end-to-end

Действует, когда `deviceType in ["heater", "link_ii"]`.

```
1. Пользователь запускает печать на принтере Bambu (через приложение Bambu, напрямую, как угодно).
2. Принтер Bambu публикует в device/<serial>/report: gcode_state=RUNNING, tray_type=ABS, tray_info_idx=..., tray_temper=240, ...
3. LINK iHeater (подписан на этот топик): парсит отчёт, извлекает текущий филамент и состояние.
4. LINK → UART → MCU iHeater: "Печать ABS идёт, tray_temper 240°C".
5. MCU iHeater: применяет свою таблицу "ABS → камера 45°C" и включает нагреватель камеры.
6. LINK параллельно публикует idryer/<serial>/integrations/status с printerState/currentFilament для UI.
7. Принтер заканчивает (gcode_state=FINISH) → LINK → MCU → MCU отключает нагрев.

Backend при этом commands/bambu_apply НЕ публикует (deviceType != dryer → отфильтровано на backend).
```

---

## Конфигурация

Настройки приходят в LINK через общий канал `commands/link_integration`. Детали — [03-link-integrations-overview.md](03-link-integrations-overview.md).

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

Параметр | Обязательно | Примечание
---|---|---
`ip` | да | IP принтера (LAN). LINK и принтер должны быть в одной сети |
`serial` | да | serial number принтера |
`lanAccessCode` | да | 8-значный код из меню принтера |
`defaultAmsId` | нет, дефолт `255` | AMS по умолчанию (`255` = текущий активный) |
`defaultTrayId` | нет, дефолт `254` | Tray по умолчанию (`254` = без AMS, external spool) |
`autoApplyOnTagDetect` | нет, дефолт `true` | если `false`, LINK игнорирует `bambu_apply` и не применяет автоматически |

### LAN Mode на принтере

Для работы интеграции пользователь должен:

1. На принтере: Settings → **LAN Only Mode** → запомнить код.
2. Включить «LAN Mode Liveview».

В новых прошивках Bambu часть API может требовать Developer Mode — на X1C это отдельная настройка. Если Developer Mode выключен, `ams_filament_setting` всё равно работает, но другие команды могут не пройти.

---

## Подключение к принтеру Bambu

### Транспорт

- **MQTT TLS** (обязательно, не plain).
- Host: `ip` из конфига.
- Port: **8883**.
- Client ID: любая строка, например `idryer_<serial>_bambu`.
- Username: `bblp`.
- Password: `lanAccessCode`.
- CA: **self-signed** у Bambu. В коде LINK `WiFiClientSecure::setInsecure()` для LAN режима — это единственный рабочий путь, подтверждено в OpenSpool.

### Топики принтера

- **Subscribe** (LINK читает): `device/<printerSerial>/report` — периодические отчёты со статусом.
- **Publish** (LINK пишет): `device/<printerSerial>/request` — команды принтеру.

### Reconnect

LINK переподключается при разрыве. Экспоненциальный backoff начиная с 1 с, потолок 60 с.

---

## Команда 1: `commands/bambu_apply` — только для Writer-режима (Dryer)

Применение настроек филамента в принтер. Приходит только Dryer-устройствам. Backend портала фильтрует по `deviceType`.

### Топик

```
idryer/<serial>/commands/bambu_apply
QoS: 1
```

### Payload (от backend)

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

Поле | Обязательно | Примечание
---|---|---
`amsId` | нет | `null` → LINK берёт `defaultAmsId` из NVS |
`trayId` | нет | `null` → LINK берёт `defaultTrayId` из NVS |
`trayType` | да | `"PLA"`, `"PETG"`, `"ABS"`, `"TPU"`, `"ASA"`, `"PC"`, `"PA"`, ... |
`colorHex` | да | 8 hex-символов RGBA (`FFAABBFF`); либо 6 — LINK добавит `FF` |
`nozzleTempMin` | да | целое, °C |
`nozzleTempMax` | да | целое, °C |
`trayInfoIdx` | да | 5-символьный Bambu код материала (например `"GFL99"` = PLA Basic Generic) |
`settingId` | нет, дефолт `""` | идентификатор пресета, обычно пустая строка |
`spoolId` | нет | UUID спула в БД портала — для диагностики и status |
`uid` | нет | UID метки — для диагностики |

Логика заполнения полей backend-ом (из OpenSpool):

- `trayType` = `filamentType.code`.
- `colorHex` = `filamentSpec.colorHex || materialSnapshot.colorHex`.
- `nozzleTempMin/Max` = ±5% от `printPreset.printNozzleTemp`.
- `trayInfoIdx` = `getBambuCode(type, brand)` — таблица в коде backend.
- `settingId` = `""`.

### Поведение LINK при получении

1. Проверить: `active == "bambu"` и секция bambu в NVS `configured`.
2. Если `active != "bambu"` или `!configured`: опубликовать `integrations/status` с `state: "config_missing"` или `state: "disabled"`, команду **игнорировать**.
3. Проверить `autoApplyOnTagDetect`: если `false` → игнорировать (apply можно вызвать только вручную).
4. Разрешить `amsId`/`trayId` (из payload или из NVS).
5. Построить Bambu MQTT payload `ams_filament_setting`:
   ```json
   {
     "print": {
       "sequence_id": "1",
       "command": "ams_filament_setting",
       "ams_id": <amsId>,
       "tray_id": <trayId>,
       "tray_info_idx": "<trayInfoIdx>",
       "tray_color": "<colorHex>",
       "nozzle_temp_min": <nozzleTempMin>,
       "nozzle_temp_max": <nozzleTempMax>,
       "tray_type": "<trayType>",
       "setting_id": "<settingId>"
     }
   }
   ```
6. Опубликовать в `device/<printerSerial>/request`.
7. Опубликовать `integrations/status` с `state: "apply_ok"` или `"apply_failed"` + `lastApply`.

Backend **всегда** шлёт `bambu_apply` при успешном резолве спула — решение о применении принимает LINK. Это позволяет изменить `autoApplyOnTagDetect` без координации с порталом.

---

## Команда 2: `commands/bambu_test` — ручной apply из портала

Ручное применение настроек катушки, инициируемое пользователем из портала. Соответствует «Сценарию Б» выше.

### Топик

```
idryer/<serial>/commands/bambu_test
QoS: 1
```

### Payload

Идентичен `commands/bambu_apply`:

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

### Поведение LINK при получении

1. Проверить: `active == "bambu"` и Bambu `configured` в NVS.
2. Если нет → опубликовать `integrations/status` с `state: "config_missing"` или `"disabled"`, **не применять**.
3. **Игнорировать** `autoApplyOnTagDetect` — ручной вызов всегда выполняется при наличии конфига.
4. Разрешить `amsId`/`trayId` (из payload или NVS-defaults).
5. Построить и опубликовать Bambu payload `ams_filament_setting` (формат тот же, что у `bambu_apply`).
6. Опубликовать `integrations/status` с `state: "apply_ok"` или `"apply_failed"`.

### Отличие от `bambu_apply`

| | `bambu_apply` | `bambu_test` |
|---|---|---|
| Триггер | Автоматически по детекту RFID | Пользователь нажал кнопку в портале |
| `autoApplyOnTagDetect` | Учитывается | Игнорируется |
| Payload принтера | Идентичен | Идентичен |

---

## Reader-режим (iHeater): чтение статуса принтера

Для `deviceType in ["heater", "link_ii"]` — **основной** режим работы. LINK никогда не шлёт в принтер (не имеет `bambu_apply` как источника), только подписан и читает.

Задача: определить, **какой филамент сейчас печатается**, и передать эту информацию MCU, чтобы MCU iHeater решил температуру камеры (таблица «материал → температура камеры» — в прошивке MCU).

### Что LINK читает из отчёта

Bambu отправляет `device/<serial>/report` с периодом ~1 Гц (или при изменениях). LINK парсит:

- `gcode_state` — состояние печати (`IDLE` / `PREPARE` / `RUNNING` / `PAUSE` / `FINISH` / `FAILED`).
- `ams.tray[N].tray_type` — тип филамента текущего tray (когда `RUNNING`).
- `ams.tray[N].tray_info_idx` — Bambu-код материала.
- `ams.tray[N].tray_color` — цвет в hex.
- `ams.tray[N].nozzle_temp_min/max` — температура сопла.
- `bed_temper` / `bed_target_temper` — температура стола.
- `nozzle_temper` / `nozzle_target_temper` — температура сопла.
- `mc_percent`, `mc_remaining_time`, `layer_num`, `total_layer_num` — прогресс.

### Что LINK отдаёт MCU iHeater

Через UART. Конкретный формат — задача прошивки iHeater-LINK, но должен содержать минимум:

- `printerState`: enum (idle/printing/paused/finished/error)
- `currentFilament.type`: строка (PLA/PETG/ABS/...)
- `currentFilament.nozzleTempMax`: температура
- (Опционально) `progress`, `bedTemp`, `nozzleTemp` — если MCU хочет их показывать на экране.

### Как реагирует MCU iHeater

Это **бизнес-логика прошивки iHeater**, не библиотеки:

- При `printerState == "printing"` → подобрать температуру камеры по таблице материал→температура.
- Включить нагрев камеры с целевой температурой.
- При `printerState == "finished" / "error" / "idle"` → отключить нагреватель.

LINK в этом режиме — только **транспорт статуса**. Логика нагрева — на MCU.

### Пользователь может override вручную

Для `deviceType: heater` / `link_ii` остаются доступны **два других источника цели камеры** (см. также [06-moonraker-printer.md](06-moonraker-printer.md) — там три источника одинаково работают и для Bambu):

- **`commands/drying`** от портала (плашка на дашборде; тот же JSON, что у iDryer).
- **Локальное меню iHeater**.

Приоритет между автоматом (Bambu `tray_type` + таблица MCU) и ручными источниками — политика прошивки iHeater-MCU.

## Общее: публикация статуса в iDryer-облако

Независимо от режима, LINK публикует секцию `bambu` в общем `integrations/status`:

```json
"bambu": {
  "configured": true,
  "enabled": true,
  "state": "online",
  "printerIp": "192.168.1.50",
  "printerSerial": "039D09C40012345",
  "printerState": "RUNNING",
  "progress": 42,
  "remainingSeconds": 3600,
  "currentLayer": 120,
  "totalLayers": 285,
  "nozzleTemp": 220.5,
  "nozzleTarget": 220,
  "bedTemp": 60.2,
  "bedTarget": 60,
  "lastApply": {
    "at": "2026-04-19T12:00:00Z",
    "result": "ok",
    "spoolId": "uuid-abc-123",
    "amsId": 0,
    "trayId": 1
  },
  "lastError": "",
  "updatedAt": "2026-04-19T12:00:12Z"
}
```

### Маппинг полей Bambu report → status

Bambu `gcode_state` → `printerState`:

- `"IDLE"` → `"idle"`
- `"PREPARE"` → `"prepare"`
- `"RUNNING"` → `"printing"`
- `"PAUSE"` → `"paused"`
- `"FINISH"` → `"finished"`
- `"FAILED"` → `"error"`

Bambu `mc_percent` → `progress` (0–100).
Bambu `mc_remaining_time` → `remainingSeconds` (уже в секундах, может прийти в минутах — проверить в рантайме).
Bambu `layer_num`, `total_layer_num` → `currentLayer`, `totalLayers`.
Bambu `nozzle_temper`, `nozzle_target_temper`, `bed_temper`, `bed_target_temper` → соответствующие поля.

### Запрос полного снэпшота

Сразу после подключения LINK шлёт:

```json
{ "pushing": { "sequence_id": "1", "command": "pushall" } }
```

в `device/<printerSerial>/request` — принтер ответит полным отчётом.

---

## Обработка ошибок

| Ошибка | Действие LINK | `state` | `lastError` |
|--------|----------------|---------|-------------|
| Нет WiFi | Reconnect когда появится | `connecting` | `"no wifi"` |
| TLS handshake failed | Retry с backoff | `error` | `"tls handshake failed"` |
| Auth rejected | Не переподключаться автоматически | `error` | `"auth rejected (check lan access code)"` |
| Принтер не отвечает на request | retry, отслеживать `pushing.pushall` | `online` + `lastError` | `"no response to request"` |
| Apply failed | Опубликовать `apply_failed` в status | `online` + `lastError` | `"apply failed: ..."` |

---

## Ограничения MVP

- Один принтер Bambu на один LINK.
- Только LAN Mode (не cloud).
- Только `ams_filament_setting`; другие команды (pause/resume/stop печати) — не в MVP.
- Фронтенд не редактирует Bambu-специфичные поля филамента отдельно — они живут в spool/material profile, backend сам готовит payload.

---

## Что дальше

- [03-link-integrations-overview.md](03-link-integrations-overview.md) — общий контракт.
- [06-moonraker-printer.md](06-moonraker-printer.md) — Moonraker (альтернатива Bambu для Klipper).
- [07-portal-integration-contract.md](07-portal-integration-contract.md) — сжатый API reference для портального разработчика.
