# Moonraker (Klipper): интеграция с принтером

LINK подключается к Moonraker-хосту пользователя (Raspberry Pi с Klipper) через WebSocket. В MVP реализован только **Reader-режим** — чтение статуса печати. Writer-режим (применение настроек катушки из портала) — запланирован.

## Целевые устройства и назначение

| `deviceType` | Назначение | MVP |
|--------------|-----------|-----|
| `Heater` / `LinkII` | **Основной сценарий.** iHeater читает **целевую температуру камеры из `VIRTUAL_CHAMBER.target`** → MCU греет камеру до этой температуры. | ✅ |
| `Dryer` | Читать статус печати для UI + **будущий Writer-режим**: применять параметры катушки из портала. | Reader ✅ / Writer: запланирован |

Контракт `commands/link_integration` с `type: "moonraker"` — одинаковый для всех `deviceType`. LINK сам решает, что делать со статусом: передавать MCU (iHeater) или публиковать для отображения (iDryer).

## Writer-режим: применение катушки в принтер

!!! warning "Не реализовано — требуется заглушка в LINK"
    Поведение должно быть **аналогично Bambu Writer-режиму**: те же команды (`bambu_apply`, `bambu_test`), тот же поток, только транспорт другой. LINK при активной Moonraker-интеграции обязан принять команду и либо выполнить, либо явно ответить `"not_implemented"`. Молча игнорировать — нельзя.

### Целевое поведение (зеркало Bambu)

Когда `active == "moonraker"` и LINK получает `bambu_apply` или `bambu_test`:

```
Катушка в сушилке → RP2040 читает метку → LINK → портал
  (то же самое, что при Bambu — портал не знает какой принтер)

Портал → idryer/<serial>/commands/bambu_apply
  (тот же payload, тот же топик — портал не меняется)

LINK получает bambu_apply, видит active == "moonraker":
  autoApplyOnTagDetect == false → молча игнорирует  (так же, как Bambu)
  Moonraker не configured      → publishes status { state: "config_missing" }
  Всё ок → отправляет параметры катушки в Klipper через Moonraker API
           → publishes status { state: "apply_ok" / "apply_failed", lastApply: {...} }
```

Флаг `autoApplyOnTagDetect` и `bambu_test` (ручной apply) работают **идентично** Bambu — разница только в транспорте на последнем шаге.

### Транспорт: Moonraker API

Klipper не имеет встроенной команды типа `ams_filament_setting`. Параметры передаются через G-code-макрос.

**LINK отправляет** POST-запрос в Moonraker:

```
POST http://<moonraker_host>/printer/gcode/script
Content-Type: application/json

{ "script": "SET_MATERIAL TYPE=PLA TEMP_MIN=190 TEMP_MAX=240 COLOR=F95959FF" }
```

**Пользователь добавляет** в `printer.cfg` макрос-приёмник:

```ini
[gcode_macro SET_MATERIAL]
gcode:
  {% set type     = params.TYPE     | default('') %}
  {% set temp_min = params.TEMP_MIN | default(0)  | int %}
  {% set temp_max = params.TEMP_MAX | default(0)  | int %}
  {% set color    = params.COLOR    | default('') %}
  ; здесь можно, например, сохранить в переменные или вывести на экран
  { action_respond_info("Material: " + type + " " + temp_min|string + "-" + temp_max|string + "C " + color) }
```

Поля из `bambu_apply` payload → параметры макроса:

| Поле payload | Параметр макроса | Пример |
|---|---|---|
| `trayType` | `TYPE` | `PLA` |
| `nozzleTempMin` | `TEMP_MIN` | `190` |
| `nozzleTempMax` | `TEMP_MAX` | `240` |
| `colorHex` | `COLOR` | `F95959FF` |

### Статус реализации в LINK

**Сейчас:** при `active == "moonraker"` команды `bambu_apply` / `bambu_test` не обрабатываются.

**Минимальная заглушка (должна быть реализована):**

```cpp
// При получении bambu_apply / bambu_test, если active == Moonraker:
HAL_LOG_WARN("LINK_MGR", "spool apply: Moonraker writer not implemented");
publishStatus(); // state: "not_implemented"
```

**Полная реализация** — изменения только в LINK: добавить HTTP-клиент и обработчик в `handleBambuApplyCommand`. Backend, портал и топики не меняются.

Источники:
- [Moonraker API: gcode/script](https://moonraker.readthedocs.io/en/latest/web_api/#run-a-gcode)
- [Klipper: gcode_macro](https://www.klipper3d.org/Command_Templates.html)

## Как iHeater получает температуру камеры — VIRTUAL_CHAMBER macro

!!! note "Проверено в `iHeater-link`"
    Эта схема уже работает на живом стенде. Полный гайд для пользователя: [/docs/iHeater-link/virtual_chamber_guide.md](../../../../iHeater-link/virtual_chamber_guide.md).

### Идея в двух предложениях

Пользователь ставит на Klipper-хост специальный `[gcode_macro VIRTUAL_CHAMBER]` с переменной `target`. В стартовом G-code слайсера пишет `M141 S50` — Klipper через обёртку-макрос записывает `50` в `VIRTUAL_CHAMBER.target`. LINK подписан на это значение и сразу передаёт MCU: «камера 50°C».

### Что должен сделать пользователь

Минимальный вариант (без датчика температуры камеры):

```ini
[gcode_macro VIRTUAL_CHAMBER]
variable_target: 0
variable_temperature: 0
variable_has_sensor: 0
gcode:

[gcode_macro M141]
gcode:
  {% set t = params.S|default(0)|float %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE={t}

[gcode_macro M191]
gcode:
  {% set t = params.S|default(0)|float %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE={t}
```

С датчиком (рекомендовано): добавить `delayed_gcode`, который каждые 2 секунды обновляет `variable_temperature` из реального `[temperature_sensor chamber]` в `printer.cfg`:

```ini
[gcode_macro VIRTUAL_CHAMBER]
variable_target: 0
variable_temperature: 0
variable_has_sensor: 1        ; 1 = датчик есть, LINK может использовать temperature
gcode:

[delayed_gcode UPDATE_VIRTUAL_CHAMBER_TEMP]
initial_duration: 2
gcode:
  {% set t = printer["temperature_sensor chamber"].temperature|default(0) %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=temperature VALUE={t}
  UPDATE_DELAYED_GCODE ID=UPDATE_VIRTUAL_CHAMBER_TEMP DURATION=2
```

Замените `temperature_sensor chamber` на имя вашего объекта сенсора в Klipper.

Почему `M141`/`M191`:

- `M141 S<temp>` — стандартная G-code команда «поставить цель камеры».
- `M191 S<temp>` — «поставить цель и подождать достижения».
- Большинство слайсеров (Orca, SuperSlicer, Cura) умеют вставлять их в стартовый G-code.
- Пользователь **не меняет свой слайсер** — только активирует эту команду в шаблоне профиля материала.

Зачем `temperature` и `has_sensor`:

- `target` — куда греть (setpoint). Ставится из `M141`/`M191`.
- `temperature` — текущая температура камеры (feedback) для PID на стороне MCU iHeater.
- `has_sensor` — признак что `temperature` валидна. `0` → LINK не читает значение как достоверное, MCU работает по своему датчику.

### Что делает LINK iHeater

1. При `active == "moonraker"` подписывается на три поля `gcode_macro VIRTUAL_CHAMBER`: `target`, `temperature`, `has_sensor`.
2. Получает начальный snapshot, запоминает все три.
3. Слушает `notify_status_update` — при изменении любого поля вызывает `VirtualChamberCallback` с полной структурой `VirtualChamberData`.
4. Передаёт MCU через UART: `target`, `temperature`, `hasSensor`.
5. MCU iHeater:
   - `target > 0` + `hasSensor == true` → PID замыкается на температуру из Klipper.
   - `target > 0` + `hasSensor == false` → MCU работает по своему локальному датчику (fallback).
   - `target == 0` → нагрев отключается.

Таблица «материал → температура» **не нужна**. Принтер (через пользователя/слайсер) сам назначает целевое значение.

### Проверенный обмен (JSON-RPC)

Запрос (начальный снимок):

```json
{
  "jsonrpc": "2.0",
  "method": "printer.objects.query",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target", "temperature", "has_sensor"]
    }
  },
  "id": 1
}
```

Ответ:

```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": {
      "gcode_macro VIRTUAL_CHAMBER": {
        "target": 50.0,
        "temperature": 27.85,
        "has_sensor": 1
      }
    },
    "eventtime": 3201823.125032914
  },
  "id": 1
}
```

Подписка на изменения (вместо polling):

```json
{
  "jsonrpc": "2.0",
  "method": "printer.objects.subscribe",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target", "temperature", "has_sensor"]
    }
  },
  "id": 1
}
```

Moonraker в ответ пришлёт snapshot + дальше — notification при изменении любого из трёх полей:

```json
{
  "jsonrpc": "2.0",
  "method": "notify_status_update",
  "params": [
    { "gcode_macro VIRTUAL_CHAMBER": { "temperature": 28.12 } },
    <eventtime>
  ]
}
```

Notification содержит только изменившиеся поля. LINK хранит последний полный snapshot и мёржит изменения.

### Если у пользователя нет VIRTUAL_CHAMBER

LINK подписывается всё равно (ошибки от Moonraker не будет — просто объект `gcode_macro VIRTUAL_CHAMBER` не найдётся и в ответе будет пусто). В этом случае `target` трактуется как `0` — камера не греется. В `integrations/status.moonraker.lastError` прописывается подсказка: `"VIRTUAL_CHAMBER macro not configured in Klipper"`.

UI портала может показать пользователю: «Для автоматического управления камерой установите `virtual_chamber.cfg` в Klipper — инструкция».

### Ручной режим

Без `VIRTUAL_CHAMBER` пользователь всё равно может управлять камерой вручную из меню iHeater (локально задать температуру). Moonraker-интеграция в этом случае даёт только статус печати для UI (см. ниже), но не управляет нагревом.

!!! note "Статус"
    **Полностью design-level.** Ничего из описанного в коде LINK не реализовано. Контракт между порталом и LINK — готов и согласован. Контракт между LINK и Moonraker — стандартный Moonraker API, общеизвестен.

!!! warning "Предполагается знакомство с Klipper/Moonraker"
    Документ оперирует терминами экосистемы Klipper: `printer.cfg`, `gcode_macro`, `variable_*`, `M141/M191` G-code команды, `[temperature_sensor chamber]`, `delayed_gcode`, `printer.objects.subscribe`, JSON-RPC 2.0. Если не работали с Klipper — сначала [Klipper docs](https://www.klipper3d.org/) (минимум разделы Config + G-Code) и [Moonraker API](https://moonraker.readthedocs.io/en/latest/web_api/). WebSocket и JSON-RPC — в [glossary](../01-overview/04-glossary.md#websocket).

---

## Архитектура

```
   iDryer LINK                    Klipper host (Raspberry Pi)
  ┌────────────────┐             ┌──────────────────────────┐
  │ MoonrakerClient├──WS ws://──►│ Moonraker (port 7125)    │
  │                │             │   JSON-RPC over WS       │
  │ Status parser  │             │                           │
  │                │             │     ┌──────────┐          │
  │ → integrations │             │     │ Klipper  │          │
  │   /status      │             │     │  klippy  │          │
  └────────────────┘             │     └──────────┘          │
                                  └──────────────────────────┘
```

LINK — **клиент** уже поднятого у пользователя Moonraker. LINK ничего не устанавливает на Klipper-хост.

---

## Конфигурация

Через общий `commands/link_integration` ([03-link-integrations-overview.md](03-link-integrations-overview.md)):

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

Параметр | Обязательно | Примечание
---|---|---
`host` | да | mDNS или IP Klipper-хоста |
`port` | нет, дефолт `7125` | Moonraker стандарт |
`apiKey` | нет | `null` = open; нужен если в `moonraker.conf` есть `[authorization]` с `force_logins: true` |
`ssl` | нет, дефолт `false` | `true` → `wss://`, иначе `ws://` |
`pollIntervalMs` | нет, дефолт `1000` | интервал подписки `subscribe` или polling |

---

## Подключение

1. LINK строит URL: `ws://<host>:<port>/websocket` (или `wss://` если `ssl: true`).
2. Опционально добавляет `?token=<apiKey>` в URL при наличии ключа.
3. Открывает WebSocket, обменивается ping/pong keep-alive.
4. Reconnect с backoff при разрыве.

Moonraker использует [JSON-RPC 2.0](https://www.jsonrpc.org/specification) поверх WebSocket.

---

## Подписка на объекты

После подключения LINK подписывается на набор объектов. Для iHeater — **обязательно** `gcode_macro VIRTUAL_CHAMBER` (основное), остальное — для UI.

```json
{
  "jsonrpc": "2.0",
  "method": "printer.objects.subscribe",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target"],
      "print_stats": null,
      "virtual_sdcard": null,
      "extruder": ["temperature", "target"],
      "heater_bed": ["temperature", "target"],
      "display_status": null,
      "idle_timeout": null
    }
  },
  "id": 1
}
```

Moonraker отвечает начальным снэпшотом + дальше шлёт notifications при изменениях любого поля.

### Notification формат

```json
{
  "jsonrpc": "2.0",
  "method": "notify_status_update",
  "params": [
    {
      "print_stats": { "state": "printing", "filename": "benchy.gcode", "print_duration": 120.5 },
      "display_status": { "progress": 0.42 },
      "extruder": { "temperature": 220.5, "target": 220 }
    },
    <eventtime>
  ]
}
```

LINK парсит `params[0]` в объект status.

---

## Маппинг в `integrations/status`

Секция `moonraker` в общем status:

```json
"moonraker": {
  "configured": true,
  "enabled": true,
  "state": "online",
  "host": "klipper.local",
  "port": 7125,
  "virtualChamberAvailable": true,
  "chamberHasSensor": true,
  "chamberTarget": 50.0,
  "chamberTemperature": 27.85,
  "printerState": "printing",
  "progress": 42,
  "remainingSeconds": 3600,
  "filename": "benchy.gcode",
  "currentLayer": null,
  "totalLayers": null,
  "nozzleTemp": 220.5,
  "nozzleTarget": 220,
  "bedTemp": 60.2,
  "bedTarget": 60,
  "printDurationSeconds": 1200,
  "lastError": "",
  "updatedAt": "2026-04-19T12:00:12Z"
}
```

**Поля VIRTUAL_CHAMBER:**

| Поле | Тип | Описание |
|------|-----|----------|
| `virtualChamberAvailable` | bool | `true` — Moonraker вернул объект `gcode_macro VIRTUAL_CHAMBER`; `false` — пользователю надо добавить макрос в Klipper |
| `chamberHasSensor` | bool | `variable_has_sensor` в macro: `true` → `chamberTemperature` валидна (feedback для PID) |
| `chamberTarget` | float, °C | `variable_target` — целевая температура камеры; `0` = выключено |
| `chamberTemperature` | float, °C | `variable_temperature` — текущая температура; валидна только при `chamberHasSensor == true` |

### Периодичность публикации

Чтобы retained-снимок оставался свежим (особенно `chamberTemperature`), LINK переотправляет `integrations/status` **раз в 30 секунд** даже если ничего не менялось. При любом структурном изменении (target / hasSensor / available / другие поля) snapshot уходит мгновенно и сбрасывает 30-секундный таймер.

### Маппинг Moonraker → status

Moonraker | status поле | Примечание
---|---|---
`print_stats.state` | `printerState` | `"standby"`, `"printing"`, `"paused"`, `"complete"`, `"cancelled"`, `"error"` |
`display_status.progress` | `progress` | Moonraker даёт 0–1, LINK публикует 0–100 |
`print_stats.filename` | `filename` |  |
`print_stats.print_duration` | `printDurationSeconds` |  |
`virtual_sdcard.progress` | альтернатива `progress` если display_status пусто |
`extruder.temperature` | `nozzleTemp` |  |
`extruder.target` | `nozzleTarget` |  |
`heater_bed.temperature` | `bedTemp` |  |
`heater_bed.target` | `bedTarget` |  |

### `remainingSeconds`

Moonraker не даёт «remaining» напрямую — надо вычислить: `print_duration / progress * (1 - progress)`. Либо брать из прошивки gcode metadata (`print.stats.estimated_time`). В MVP — вычисление из progress, `null` если progress = 0.

### `currentLayer` / `totalLayers`

Klipper по умолчанию не знает layer — слои зависят от слайсера. Если gcode содержит `SET_PRINT_STATS_INFO TOTAL_LAYER=N CURRENT_LAYER=i` (современные слайсеры пишут), Moonraker отдаст в `print_stats.info`:

```json
"print_stats": {
  "info": { "total_layer": 285, "current_layer": 120 }
}
```

LINK кладёт в `currentLayer`/`totalLayers`. При отсутствии — `null`.

---

## Ошибки и коды состояния

| Ошибка | `state` | `lastError` |
|--------|---------|-------------|
| WiFi нет | `connecting` | `"no wifi"` |
| Host не резолвится | `error` | `"host not resolved"` |
| TCP refused | `error` | `"connection refused (port 7125?)"` |
| WS handshake failed | `error` | `"ws handshake failed"` |
| `401 Unauthorized` | `error` | `"auth failed (check apiKey)"` |
| JSON-RPC method error | `online` + lastError | `"method error: <msg>"` |
| Разрыв WS | `connecting` | `"reconnecting"` |

---

## Команды LINK → Klipper (опционально, не MVP)

Moonraker поддерживает вызовы `printer.gcode.script`, `printer.print.start/pause/resume/cancel`. В MVP LINK **не посылает** ничего, только читает. Если понадобится в будущем — добавим отдельную MQTT-команду `commands/printer_command`.

---

## Использование статуса

### iHeater (`deviceType: heater / link_ii`) — основной потребитель

LINK iHeater передаёт через UART в MCU главное поле — **`chamberTarget` из `VIRTUAL_CHAMBER.target`**.

Дополнительно может прокидывать (для отображения на экране iHeater): `printerState`, `progress`, `nozzleTarget`, `bedTarget`. Это опционально — для управления нагревом достаточно одного `chamberTarget`.

MCU iHeater:

- `chamberTarget > 0` → включить нагреватель камеры с целью = `chamberTarget`.
- `chamberTarget == 0` → выключить.

**Таблица «материал → температура» не нужна.** Пользователь задаёт целевую температуру камеры в слайсере (через `M141`/`M191` в стартовом G-code), и принтер передаёт её через `VIRTUAL_CHAMBER.target`.

Портал в этом цикле не участвует. Параллельно LINK публикует `integrations/status` — для UI в приложении (отображение `chamberTarget` и прогресса печати).

### Три независимых входа для target камеры iHeater

MCU iHeater одновременно может получать цель камеры из трёх источников — протокол их разделяет, **приоритет решает прошивка MCU**:

1. **`VIRTUAL_CHAMBER.target`** (Moonraker → LINK → UART) — автомат при печати.
2. **`commands/drying`** от портала (стандартная команда MQTT, тот же `CommandPayload`, что у iDryer; `params.temperature` = цель камеры, `params.duration = 0` = бессрочно). См. [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md).
3. **Локальное меню на экране iHeater**.

Рекомендуемый приоритет в прошивке iHeater: автомат (VIRTUAL_CHAMBER) во время активной печати перебивает ручные значения; между `drying` и меню — последний-пишет. Это **политика прошивки**, не контракта.

### iDryer (`deviceType: dryer`) — статус для UI

В MVP LINK iDryer только публикует `integrations/status` с данными Moonraker. Фронтенд может:

- Показывать «идёт печать PLA, камера U1 сушит этот же PLA» в UI.
- Автоматически запускать сушку камеры с филаментом, соответствующим `filename` (через ручной маппинг или эвристику) — если эту логику реализовать на backend/frontend.
- Останавливать сушку при `printerState = "complete"`.

Эта логика — **на бэкенде/фронтенде портала**, не на LINK. LINK просто раздаёт статус.

---

## Ограничения MVP

- Один Moonraker-хост на один LINK.
- Только чтение, не управление.
- `currentLayer`/`totalLayers` — только если слайсер пишет соответствующие metadata в gcode.
- Klipper без Moonraker (прямое Klippy-API) — не поддерживается.

---

## Что дальше

- [03-link-integrations-overview.md](03-link-integrations-overview.md) — общий контракт.
- [05-bambu-integration.md](05-bambu-integration.md) — альтернатива для владельцев Bambu.
- [07-portal-integration-contract.md](07-portal-integration-contract.md) — сжатый reference для портального разработчика.
