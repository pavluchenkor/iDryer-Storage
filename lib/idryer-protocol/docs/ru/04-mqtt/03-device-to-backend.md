# MQTT: JSON от устройства в облако

Форматы публикаций в топики `idryer/<serial>/...`. Все примеры — реальные, отражают то, что пишет `TelemetryPublisher` и `MqttClient`.

!!! note "Источник правды"
    `src/cloud/telemetry_publisher.cpp` и `src/mqtt/mqtt_client.cpp` (`publishInfo`). Любое расхождение — баг документации.

---

## `info` (retained)

Статическая информация об устройстве. Публикуется **один раз** после `Online`. Повторная публикация — только при изменении конфигурации (перезагрузка с новым `unitsCount`, новая версия прошивки).

```json
{
  "hardwareVersion": "v1.0",
  "firmwareVersion": "1.2.3",
  "workTimeCounter": 360000,
  "unitsCount": 2,
  "mcuSerial": "36B955AB4350FEDC",
  "deviceType": "dryer",
  "units": [
    {
      "unitId": 0,
      "capabilities": {
        "heater": true,
        "fan": true,
        "servo": false,
        "RhAirSensor": true,
        "TempAirSensor": true,
        "TempHeaterSensor": true
      },
      "scales": [0, 1],
      "rfid": [0]
    },
    {
      "unitId": 1,
      "capabilities": {
        "heater": true,
        "fan": true,
        "servo": false,
        "RhAirSensor": true,
        "TempAirSensor": true,
        "TempHeaterSensor": false
      },
      "scales": [2, 3],
      "rfid": [1]
    }
  ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

**Поля:**

| Поле | Тип | Описание |
|------|-----|----------|
| `hardwareVersion` | string | Из `HelloPayload.hardwareVersion` |
| `firmwareVersion` | string | Форматированная версия `"MAJOR.MINOR.PATCH"` |
| `workTimeCounter` | int | Секунды наработки |
| `unitsCount` | int | 0–4 |
| `mcuSerial` | string | 16 hex-символов MCU (только если задан) |
| `deviceType` | string | `"dryer"`, `"heater"`, `"telemetry"`, `"link"`, `"link_ii"`; **отсутствует** для legacy-прошивок |
| `units[]` | array | Конфигурация камер (только «живые», `unitsCount` штук) |
| `units[i].unitId` | int | 0–3 |
| `units[i].capabilities.*` | bool | Флаги железа (heater, fan, servo, RhAirSensor, TempAirSensor, TempHeaterSensor) |
| `units[i].scales[]` | array int | Индексы привязанных датчиков весов (`0xFF` отфильтрованы) |
| `units[i].rfid[]` | array int | Индексы привязанных RFID-ридеров (`0xFF` отфильтрованы) |
| `timestamp` | ISO 8601 UTC | Момент публикации |

!!! note "deviceType для legacy"
    Если MCU не заполняет поле `deviceType` в `HelloPayload` (значение `0x00`), LINK не кладёт это поле в JSON вовсе. Портал в этом случае трактует устройство как `"dryer"` с `unitsCount` камерами.

!!! warning "unitId в info — число, а не строка"
    В `info` поле `units[].unitId` — это **число 0–3** (сырое значение из `UnitConfig.unitId`). В остальных топиках (`telemetry`, `status`, `weights`, `rfid`, команды) `unitId` — это **строка `"U1"`…`"U4"`**. Такая несимметричность историческая; учитывайте её при парсинге на бэкенде.

---

## `telemetry` (QoS 0, не retained)

Текущие измерения. Публикуется по таймеру прикладного кода (обычно 5 секунд).

```json
{
  "units": [
    {
      "unitId": "U1",
      "temperature": 55.3,
      "humidity": 45.2,
      "heaterPower": 80,
      "fanStatus": true
    }
  ]
}
```

| Поле | Тип | Примечание |
|------|-----|------------|
| `units[].unitId` | string | `"U1"`…`"U4"` (индекс + 1, преобразован из UART `unitId: 0..3`) |
| `units[].temperature` | float | `temperatureC10 / 10`, 1 знак после запятой |
| `units[].humidity` | float | `humidityPct10 / 10`, 1 знак |
| `units[].heaterPower` | int | 0–100 |
| `units[].fanStatus` | bool | из `fanOn` (0/1) |

Количество элементов в `units[]` равно `count` в UART-пакете (1–4). Если в UART-пакете `count=1`, в JSON будет один элемент — массив заполненных камер, не фиксированная длина 4.

---

## `status` (QoS 1, retained)

Режим работы и таймеры. Публикуется **по событию** (изменение режима), не по интервалу.

### Idle / Fault — краткая форма

```json
{
  "units": [{ "unitId": "U1", "mode": "IDLE" }],
  "uptime": 12345
}
```

### Активный режим (Drying / Storage)

```json
{
  "units": [
    {
      "unitId": "U1",
      "mode": "DRYING",
      "sessionNum": 42,
      "target": {
        "temperature": 55.0,
        "duration": 120,
        "humidity": 15
      },
      "totalElapsed": 360,
      "totalRemaining": 6840
    }
  ],
  "uptime": 12345
}
```

`target.humidity` добавляется, только если `targetHumidityPct > 0` (релевантно для STORAGE).

### Профильный режим

```json
{
  "units": [
    {
      "unitId": "U1",
      "mode": "PROFILE",
      "sessionNum": 42,
      "target": { "temperature": 80.0, "duration": 0 },
      "totalElapsed": 4200,
      "totalRemaining": 10200,
      "currentStage": 1,
      "totalStages": 3,
      "stageElapsed": 600,
      "stageRemaining": 7200,
      "stagePhase": "HOLD"
    }
  ],
  "uptime": 12345
}
```

| Поле | Описание |
|------|----------|
| `mode` | `"IDLE"`, `"DRYING"`, `"STORAGE"`, `"PROFILE"`, `"FAULT"` |
| `sessionNum` | Номер сессии; в Idle/Fault отсутствует |
| `target.temperature` | °C |
| `target.duration` | минуты (0 = бесконечно, для STORAGE) |
| `target.humidity` | % (только для STORAGE) |
| `totalElapsed` | секунд с начала режима |
| `totalRemaining` | секунд до конца программы |
| `currentStage` | индекс этапа (0-based, только PROFILE) |
| `totalStages` | всего этапов (только PROFILE) |
| `stageElapsed`, `stageRemaining` | секунд текущего этапа (PROFILE) |
| `stagePhase` | `"RAMP"` или `"HOLD"` (PROFILE) |
| `uptime` | аптайм устройства, секунды |

---

## `weights` (QoS 1, не retained)

```json
{
  "weights": [
    { "sensorId": "W1", "value": 823.4, "unitId": "U1" }
  ]
}
```

| Поле | Тип | Примечание |
|------|-----|------------|
| `sensorId` | string | `"W1"`…`"W4"` |
| `value` | float | граммы, `weightGramsC10 / 10` |
| `unitId` | string | `"U1"`…`"U4"` — привязка сенсора к камере |

Публикация — по интервалу или изменению веса (обычно раз в 10 секунд).

---

## `rfid` (QoS 1, retained)

### Событие tag_detected / tag_removed

```json
{
  "unitId": "U1",
  "event": "tag_detected",
  "tag": "DEADBEEF12345678",
  "readerId": 0
}
```

| Поле | Тип | Примечание |
|------|-----|------------|
| `unitId` | string | `"U1"`…`"U4"` |
| `event` | string | `"tag_detected"` или `"tag_removed"` |
| `tag` | string | HEX ID метки (для `tag_removed` — пустая) |
| `readerId` | **number** | 0–3 (сырой индекс, не `"R1"`) |

!!! warning "readerId — число, не строка"
    В отличие от `unitId`/`sensorId`, поле `readerId` в MQTT остаётся **числом** (0–3). Если вы парсите на бэкенде, не ожидайте строку `"R1"`.

### Данные метки (RFID read)

На команду `commands/read_rfid` LINK собирает фрагменты `RfidReadData` и публикует в тот же топик `rfid`:

```json
{
  "readerId": 0,
  "unitId": "U1",
  "tagId": "DEADBEEF12345678",
  "format": "unknown",
  "data": "BASE64_STRING..."
}
```

| Поле `format` | Когда | `data` |
|---------------|-------|--------|
| `"empty"` | Все байты метки = 0 — пустая метка | `null` |
| `"openprinttag"` | В первых 128 байтах найдена MIME-сигнатура `application/vnd.openprinttag` (Prusa OpenPrintTag) | base64-строка |
| `"openspool"` | В первых 128 байтах найдена MIME-сигнатура `application/vnd.openspool` | base64-строка |
| `"unknown"` | Ни одна из известных сигнатур не найдена — парсинг делает портал | base64-строка |

Детекция выполняется в `TelemetryPublisher::publishRfidData` поиском подстроки в первых 128 байтах буфера.

---

## `events` (QoS 1, не retained)

Ошибки приложения, предупреждения, диагностические события.

### Из UART `Log` (0x60)

```json
{
  "severity": "error",
  "source": "HEATER",
  "event": "OVER_MAX",
  "message": "Heater over max temperature",
  "unitId": "U1",
  "timestamp": "2026-04-19T12:00:00Z"
}
```

### Из ошибок UART-линка (референсная прошивка Link)

```json
{
  "severity": "error",
  "source": "UART",
  "event": "PROTOCOL_ERROR_LOCAL",
  "message": "CRC mismatch on seq=42",
  "timestamp": "2026-04-19T12:00:00Z"
}
```

!!! warning "Автоматической публикации нет"
    `UartBridge` при приёме `Log`-кадра **не** публикует в `events` автоматически — только вызывает `logHandler_`. Прикладной код должен сам вызвать `MqttClient::publishEvent(doc)`. Референсная прошивка Link делает это в `IdryerDevice::handleLog`. Без своего колбэка топик `events` будет пустым.

---

## `config` (QoS 1, не retained)

Полный JSON меню устройства (до ~3 КБ). Публикуется:

- в ответ на команду `commands/get_config`;
- после завершения сборки конфига из фрагментов UART.

Формат:

```json
{
  "v": 8,
  "units": 3,
  "active": 0,
  "lang": "en",
  "menu": [
    { "id": 3, "t": "val", "val": [50, 65, 85] },
    { "id": 5, "t": "val", "val": [40, 15] },
    { "id": 17, "t": "val", "val": [55, 60, 55] },
    ...
  ]
}
```

| Поле | Назначение |
|------|------------|
| `v` | Версия схемы конфига (ревизия) |
| `units` | Количество активных камер |
| `active` | Индекс активной камеры (0-based) |
| `lang` | Код языка UI, `"en"` / `"ru"` |
| `menu[]` | Массив элементов меню с их текущими значениями |

Структура и возможные значения элементов меню определяются `menu_meta.h` и согласованы между MCU и LINK на этапе сборки.

---

## `config/delta` (QoS 1, не retained)

Инкрементальное обновление меню после `set` / `invoke`:

```json
{
  "d": {
    "3": [55, 60, 55],
    "81": 3
  }
}
```

Ключ — `id` элемента меню, значение — новое содержимое. Publisher отправляет это при изменении на MCU.

---

## `offline` (LWT, не для ручной публикации)

Публикуется **брокером** при аварийном обрыве соединения LINK:

```json
{}
```

Само устройство в этот топик ничего не пишет — это всегда делает EMQX как часть механизма LWT.

---

## Общие заметки

- **Timestamp** `"YYYY-MM-DDTHH:MM:SSZ"` в UTC добавляется `MqttClient` во все JSON-публикации, где это реализовано (info, telemetry, status, events).
- **Размер JSON** — ограничен буфером `StaticJsonDocument<N>` в коде публикатора. Для `telemetry` — 1024 байта (достаточно для 4 камер).
- **Пустые массивы** — `units` никогда не бывает пустым в `info` (минимум 1 элемент, если `unitsCount >= 1`).

---

## Что дальше

- [04-backend-to-device.md](04-backend-to-device.md) — JSON-команды от облака к устройству.
- [05-examples.md](05-examples.md) — готовые `mosquitto_sub` для просмотра этих JSON на живом устройстве.
