# MQTT: команды от облака к устройству

Полный контракт топиков `idryer/<serial>/commands/<имя>`. Для каждой команды — формат JSON, какие поля обязательны, что происходит дальше (UART-кадр, локальное действие).

!!! note "Источник правды"
    `src/cloud/command_handler.cpp`, метод `handleMqttCommand` и обработчики `handleStart`, `handleStorage`, `handleProfile`, `handleStop`, `handleFind`, `handleSet`, `handleInvoke`, `handleGetConfig`, `handleReadRfid`, `handleWriteRfid`, `handleClearErrors`, `handlePing`.

---

## Общие правила

1. **Все команды** приходят в топик `idryer/<serial>/commands/<имя>`. QoS 1.
2. **Тело — JSON.** Пустой payload (`""`) допустим для команд без параметров — парсер JSON принимает пустой объект.
3. **Поле `unitId`** — всегда строка формата `"U1"`…`"U4"`. При отсутствии или кривом формате `CommandHandler` трактует как «все камеры» (UART `unitId = 0xFF`).
4. **Опциональное поле `timestamp`** (ISO 8601) в корне JSON — если задан `setTimeSyncCallback()`, он будет вызван для синхронизации RTC устройства.
5. **Команды, меняющие состояние MCU**, уходят по UART с `ackRequired = true`. MCU отвечает `CommandAck`.

---

## `drying` — запуск сушки / ручное управление температурой камеры (iHeater)

### Новый формат (рекомендованный)

```json
{
  "unitId": "U1",
  "params": {
    "temperature": 55,
    "duration": 240
  }
}
```

!!! note "Для `deviceType: heater` / `link_ii` — тот же канал"
    Эта же команда используется порталом как удалённая «плашка на дашборде» для ручного задания температуры **камеры нагревателя** iHeater. Семантика полей та же:
    - `temperature` — целевая температура камеры в °C.
    - `duration: 0` — бессрочно (работать пока не придёт `stop` или не перебьёт `VIRTUAL_CHAMBER.target`).
    - `duration: N` — автоотключение через N минут.

    Приоритет между `commands/drying`, `VIRTUAL_CHAMBER.target` из Klipper и локальным меню — бизнес-логика прошивки iHeater-MCU, не протокола. Рекомендуемый: Moonraker > drying > меню.

### Legacy-формат (без `params`)

```json
{
  "unitId": "U1",
  "targetTemperature": 55.0,
  "durationMinutes": 240
}
```

| Поле | Тип | Значение по умолчанию | Назначение |
|------|-----|------------------------|------------|
| `unitId` | string | `"U1"`..`"U4"` или отсутствие → все | Камера |
| `params.temperature` | int (°C) | 55 | Целевая температура |
| `params.duration` | int (мин) | 240 | Длительность |

**UART-эффект:** `CommandPayload` (13 байт), `command=Start`, `targetState=DryerMode::Drying(1)`, `arg0=temperature*10`, `arg1=duration`.

---

## `storage` — режим хранения

```json
{
  "unitId": "U1",
  "params": {
    "temperature": 40,
    "humidity": 15
  }
}
```

| Поле | Тип | Default | Назначение |
|------|-----|---------|------------|
| `params.temperature` | int (°C) | 40 | Целевая температура |
| `params.humidity` | int (%) | 15 | Целевая влажность |

**UART-эффект:** `CommandPayload`, `command=Start`, `targetState=DryerMode::Storage(2)`, `arg0=temperature*10`, `arg1=humidity`.

Длительность в Storage не задаётся — режим работает, пока не придёт `stop`.

---

## `profile` — многоэтапная сушка

```json
{
  "unitId": "U1",
  "mode": "PROFILE",
  "startStage": 0,
  "stages": [
    { "temp": 60,  "ramp": 300,  "hold": 1800 },
    { "temp": 100, "ramp": 600,  "hold": 6000 },
    { "temp": 70,  "ramp": 600,  "hold": 12000 }
  ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

| Поле | Тип | Обязательно | Назначение |
|------|-----|-------------|------------|
| `unitId` | string | да | `"U1"`..`"U4"` |
| `stages[]` | array | да | 1–10 этапов |
| `startStage` | int | нет, default 0 | Индекс этапа для старта (0-based) |
| `mode` | string | нет, не читается | Для документации бэкенда, парсер игнорирует |

### `stages[i]`

| Поле | Тип | Ед.изм. | Описание |
|------|-----|---------|----------|
| `temp` | int или float | °C | Целевая температура этапа |
| `ramp` | int | секунды | Время разгона; `0` = «форсированный нагрев» до цели |
| `hold` | int | секунды | Время удержания |

!!! note "Больше 10 этапов"
    Если `stages.length > 10`, LINK обрежет до 10 с предупреждением в лог и продолжит. Если меньше 1 или массив отсутствует — команда отбрасывается без эффекта.

!!! warning "startStage out-of-range"
    LINK **не валидирует** `startStage` относительно `stages.length`. Значение уходит в UART-payload как есть. Если вы указали `startStage=5` при 3 этапах, поведение определяется MCU — может быть переход в Fault, старт с 0-го этапа или иная реакция. Клиент должен самостоятельно проверять, что `0 <= startStage < stages.length`.

**UART-эффект:** `ProfilePayload` (64 байта) внутри `MessageKind::Command` (0x20). Дискриминация по длине payload — [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md#profilepayload-внутри-command-0x20--64-байта).

Семантика этапов и pseudocode PID — [../06-flows/04-profile-mode.md](../06-flows/04-profile-mode.md).

---

## `stop` — остановка

```json
{ "unitId": "U1" }
```

Без `unitId` → остановка всех камер (`unitId = 0xFF`).

**UART-эффект:** `CommandPayload`, `command=Stop`, `targetState=Idle`, `arg0=0, arg1=0`.

---

## `find` — мигание/локальный маяк

```json
{ "unitId": "U1" }
```

**UART-эффект:** `CommandPayload`, `command=Find`.

Реализация эффекта — в MCU (обычно несколько секунд мигания LED или экрана).

---

## `get_config` — запрос полного конфига меню

```json
{}
```

или

```json
{ "unitId": "U1" }
```

**UART-эффект:** `CommandPayload`, `command=GetConfig`. MCU в ответ шлёт фрагментированный `ConfigPush` с полным JSON меню. LINK собирает и публикует в `idryer/<serial>/config`.

---

## `set` — изменить параметр меню

```json
{ "id": 3, "unit": 0, "val": 55 }
```

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | int | Идентификатор элемента меню |
| `unit` | int | Камера (0-based индекс); опционально, по умолчанию текущая |
| `val` | int, float или array | Новое значение |

**UART-эффект:** JSON `{"cmd":"set","id":3,"unit":0,"val":55}` уходит как `ConfigPush` (0x30), не как `CommandPayload`. MCU применяет, обновляет локальный конфиг, публикует `config/delta` обратно.

!!! warning "Фрагментация set/invoke ограничена"
    В референсной прошивке фрагментация для больших `set`/`invoke` JSON из MQTT **не реализована**. Пакет должен помещаться в `CONFIG_CHUNK_DATA_SIZE = 194` байта после добавления 6 байт заголовка чанка. Большие массивы `val` могут не пройти. См. [../03-uart/03-message-types.md](../03-uart/03-message-types.md).

---

## `invoke` — вызвать action из меню

```json
{ "id": 5 }
```

| Поле | Тип | Описание |
|------|-----|----------|
| `id` | int | Идентификатор action-элемента меню (кнопки «выполнить XY») |

**UART-эффект:** JSON `{"cmd":"invoke","id":5}` уходит как `ConfigPush`. MCU выполняет соответствующий action.

---

## `read_rfid` — прочитать метку

```json
{ "unitId": "U1" }
```

**UART-эффект:** `CommandPayload`, `command=ReadRfid`. MCU читает метку в указанной камере и отправляет данные обратно через `RfidReadData` (0x1A). LINK собирает фрагменты и публикует base64-буфер в `idryer/<serial>/rfid` (retained).

Детали — [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## `write_rfid` — записать метку

```json
{
  "unitId": "U1",
  "data": "BASE64_STRING...",
  "verify": "header32"
}
```

Параметры:

- `unitId` — идентификатор камеры `"U1"`…`"U4"`.
- `data` — base64-строка, до 888 байт после декодирования.
- `verify` — режим сверки после записи: `"none"` / `"header32"` (по умолчанию) / `"full"`. Опциональное поле.

**UART-эффект:** LINK декодирует `data`, отправляет `Command WriteRfid` (0x08, `arg0 = rawLen`, `arg1 = verifyCode`), затем фрагменты `RfidWriteData` (0x1B) по 163 байта полезных данных. Количество фрагментов — `ceil(rawLen / 163)`.

**Flow control:** stop-and-wait ACK. Каждый кадр идёт с `FLAG_ACK_REQUIRED`, LINK не шлёт следующий до получения `CommandAck`. Таймаут на ACK — 200 мс, до 3 ретраев, потом транзакция обрывается.

Детали протокола и ограничения — [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## `clear_errors` — очистить лог ошибок

```json
{ "unitId": "U1" }
```

Без `unitId` → очищает лог для всех камер.

**UART-эффект:** `CommandPayload`, `command=ClearErrors (0x12)`.

---

## `link_integration` — настройки внешних интеграций

Сохранение credentials для HA / Bambu / Moonraker в NVS LINK. Дискриминированный union по `type`.

```json
{ "type": "ha",        "enabled": true, "host": "homeassistant.local", ... }
{ "type": "bambu",     "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..." }
{ "type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125, ... }
```

LINK сохраняет секцию в NVS. Поднимает клиента только если `activeIntegration` в меню указывает на этот тип. Детали — [../07-features/03-link-integrations-overview.md](../07-features/03-link-integrations-overview.md).

!!! note "Статус"
    Design-level. Handler не реализован в библиотеке.

---

## `bambu_apply` — применить настройки филамента в Bambu

**Публикуется только для `deviceType == "dryer"` (или legacy/Unknown).** Для Heater/LinkII backend не шлёт — они в Reader-режиме и сами читают статус принтера.

Отправляется backend-ом при `tag_detected`, содержит готовый OpenSpool-совместимый payload. LINK (если `active == "bambu"` и configured) шлёт `ams_filament_setting` в принтер.

```json
{
  "trayType": "PLA",
  "colorHex": "FFAABBFF",
  "nozzleTempMin": 209,
  "nozzleTempMax": 231,
  "trayInfoIdx": "GFL99",
  "settingId": "",
  "amsId": null,
  "trayId": null,
  "spoolId": "uuid-...",
  "uid": "AABB1234"
}
```

Детали — [../07-features/05-bambu-integration.md](../07-features/05-bambu-integration.md).

!!! note "Статус"
    Design-level. Handler не реализован в библиотеке.

---

## `ping` — проверка живости

```json
{}
```

**UART-эффект:** **никакого** — команда только логируется на стороне LINK. Используется бэкендом для проверки, что устройство онлайн и обрабатывает входящие MQTT-сообщения.

---

## Сводная таблица

| MQTT-команда | UART-кадр | Статус |
|--------------|-----------|--------|
| `drying` | `CommandPayload` (Start, Drying) | ✅ |
| `storage` | `CommandPayload` (Start, Storage) | ✅ |
| `profile` | `ProfilePayload` внутри `Command` | ✅ |
| `stop` | `CommandPayload` (Stop) | ✅ |
| `find` | `CommandPayload` (Find) | ✅ |
| `get_config` | `CommandPayload` (GetConfig) | ✅ |
| `set` | `ConfigPush` JSON `{"cmd":"set",...}` | ⚠️ фрагментация не реализована |
| `invoke` | `ConfigPush` JSON `{"cmd":"invoke",...}` | ✅ (до лимита чанка) |
| `read_rfid` | `CommandPayload` (ReadRfid) | ✅ |
| `write_rfid` | `CommandPayload` (WriteRfid) + `RfidWriteData` фрагменты (stop-and-wait ACK) | ✅ |
| `clear_errors` | `CommandPayload` (ClearErrors) | ✅ |
| `ping` | — | ✅ (только лог) |
| `link_integration` | NVS-запись на LINK | ❌ design-level |
| `bambu_apply` | MQTT-запрос в принтер Bambu | ❌ design-level |

---

## Что дальше

- [05-examples.md](05-examples.md) — готовые `mosquitto_pub` для каждой команды.
- [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md) — цикл `set/invoke` + delta на практике.
- [../06-flows/04-profile-mode.md](../06-flows/04-profile-mode.md) — профильная сушка целиком.
