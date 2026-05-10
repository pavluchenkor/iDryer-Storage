# Бинарный формат payload (нормативный справочник)

Исчерпывающее описание каждой структуры payload: поле за полем, байт за байтом. Это норматив для парсеров. Если у вас несовпадение с этой страницей — проверьте `src/uart/uart_protocol.h` как источник правды и откройте issue в доке.

!!! note "Общие правила"
    - Все структуры — `#pragma pack(1)`, без выравнивания, байты подряд.
    - Все многобайтовые числа — **little-endian**.
    - `static_assert` в `uart_protocol.h` гарантирует указанные размеры на этапе компиляции.

---

## Заголовок кадра (FrameHeader) — 6 байт

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `sof` | `0xAA` |
| 1 | 1 | uint8 | `version` | `0x01` |
| 2 | 1 | uint8 | `flags` | см. [02-frame-and-crc.md](02-frame-and-crc.md) |
| 3 | 1 | uint8 (`MessageKind`) | `kind` | тип сообщения |
| 4 | 1 | uint8 | `sequence` | счётчик 0–255 |
| 5 | 1 | uint8 | `payloadLength` | длина payload, 0–200 |

После payload — 2 байта CRC16 little-endian.

---

## HelloPayload (0x01) — 86 байт

Направление: MCU → LINK; триггер LINK → MCU (`role = 0xFF`, полезных данных нет).

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`Role`) | `role` | `0x01`=MCU, `0x02`=ESP (резерв), `0xFF`=HelloRequest |
| 1 | 1 | uint8 (`DeviceType`) | `deviceType` | см. таблицу ниже; `0`=legacy → портал считает Dryer |
| 2 | 2 | — | `_pad1[2]` | выравнивание |
| 4 | 4 | uint32 LE | `firmwareVersion` | `(MAJOR<<16) \| (MINOR<<8) \| PATCH` |
| 8 | 4 | uint32 LE | `workTimeCounter` | наработка, секунды |
| 12 | 8 | char[8] | `hardwareVersion` | ASCII строка, null-padded |
| 20 | 1 | uint8 | `unitsCount` | количество камер 0–4 |
| 21 | 48 | `UnitConfig[4]` | `units` | 4 × 12 байт; `units[unitsCount..3]` заполняют нулями |
| 69 | 17 | char[17] | `mcuSerial` | 16 hex-символов серийника + `\0` |

### `DeviceType`

| Значение | Имя | Описание |
|----------|-----|----------|
| `0x00` | `Unknown` | Legacy / поле не заполнено. Портал трактует как Dryer. |
| `0x01` | `Dryer` | iDryer; число камер берётся из `unitsCount` |
| `0x02` | `Heater` | iHeater |
| `0x03` | `Telemetry` | Модуль телеметрии |
| `0x04` | `Link` | Универсальный LINK (standalone без MCU-компаньона) |
| `0x05` | `LinkII` | Специализированный LINK для iHeater |
| `0x06..0xFF` | — | Резерв |

### `UnitConfig` (12 байт, вложено в `HelloPayload.units`)

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `unitId` | 0–3 (соответствует U1..U4 в MQTT) |
| 1 | 1 | — | `_pad1` | выравнивание для `capabilities` |
| 2 | 2 | uint16 LE | `capabilities` | битовые флаги железа (см. ниже) |
| 4 | 4 | uint8[4] | `scales` | индексы датчиков весов 0–3, `0xFF` = не используется |
| 8 | 4 | uint8[4] | `rfid` | индексы RFID-ридеров 0–3, `0xFF` = не используется |

### Флаги `capabilities`

| Бит | Маска | Константа | Описание |
|-----|-------|-----------|----------|
| 0 | `0x0001` | `HEATER` | Есть нагреватель |
| 1 | `0x0002` | `FAN` | Есть вентилятор |
| 2 | `0x0004` | `SERVO` | Есть сервопривод заслонки |
| 3 | `0x0008` | `RH_AIR_SENSOR` | Датчик влажности воздуха |
| 4 | `0x0010` | `TEMP_AIR_SENSOR` | Датчик температуры воздуха |
| 5 | `0x0020` | `TEMP_HEATER_SENSOR` | Датчик температуры нагревателя |
| 6–15 | — | — | Резерв |

---

## HelloAckPayload (0x02) — 37 байт

Направление: LINK → MCU.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 4 | uint32 LE | `ipAddress` | IP в little-endian; `0` = нет подключения |
| 4 | 33 | char[33] | `ssid` | Имя WiFi, null-terminated; `""` = нет подключения |

Пример: IP `192.168.1.5` в байтах (little-endian) — `05 01 A8 C0`.

---

## TelemetryPayload (0x10) — 29 байт

Направление: MCU → LINK. Фиксированная длина: `1 + 4×7 = 29`, независимо от `unitsCount` — заполняйте нулями неиспользуемые элементы.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `count` | Фактическое число заполненных entry (1–4) |
| 1 | 4×7 | `TelemetryEntry[4]` | `units` | Массив данных юнитов |

### `TelemetryEntry` (7 байт)

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `unitId` | 0–3 |
| 1 | 2 | int16 LE | `temperatureC10` | Температура × 10 (`553` → 55.3 °C) |
| 3 | 2 | uint16 LE | `humidityPct10` | Влажность × 10 (`452` → 45.2 %) |
| 5 | 1 | uint8 | `heaterPowerPct` | Мощность 0–100 |
| 6 | 1 | uint8 | `fanOn` | 0/1 → false/true |

---

## WeightsPayload (0x12) — 17 байт

Направление: MCU → LINK. Фиксированная длина.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `count` | 1–4 |
| 1 | 4×4 | `WeightEntry[4]` | `weights` | Массив датчиков |

### `WeightEntry` (4 байта)

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `sensorId` | 0–3 (W1=0 … W4=3) |
| 1 | 1 | uint8 | `unitId` | 0–3 — к какой камере привязан |
| 2 | 2 | uint16 LE | `weightGramsC10` | Вес × 10 (`1234` → 123.4 г), до 5000 |

---

## StatusPayload (0x13) — 133 байта

Направление: MCU → LINK. Фиксированная длина: `1 + 4×32 + 4 = 133`.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `count` | 1–4 |
| 1 | 4×32 | `StatusEntry[4]` | `units` | Статусы юнитов |
| 129 | 4 | uint32 LE | `uptime` | Uptime устройства, секунды |

### `StatusEntry` (32 байта)

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `unitId` | 0–3 |
| 1 | 1 | uint8 (`DryerMode`) | `mode` | 0=Idle, 1=Drying, 2=Storage, 3=Profile, 4=Fault |
| 2 | 4 | uint32 LE | `sessionNum` | Номер сессии; 0 для Idle/Fault |
| 6 | 2 | int16 LE | `targetTempC10` | Целевая температура × 10 |
| 8 | 2 | uint16 LE | `targetHumidityPct` | Целевая влажность % (0 = не используется) |
| 10 | 2 | uint16 LE | `durationMinutes` | Длительность, минуты (0 = бесконечно/Storage) |
| 12 | 4 | uint32 LE | `elapsedSeconds` | Секунд с начала текущей сессии |
| 16 | 4 | uint32 LE | `stageElapsedSeconds` | Секунд на текущем этапе (Profile) |
| 20 | 4 | uint32 LE | `stageRemainingSeconds` | Секунд до конца этапа (Profile) |
| 24 | 4 | uint32 LE | `totalRemainingSeconds` | Секунд до конца программы |
| 28 | 1 | uint8 | `currentStage` | Индекс текущего этапа (Profile, 0-based) |
| 29 | 1 | uint8 | `totalStages` | Всего этапов (Profile) |
| 30 | 1 | uint8 (`StagePhase`) | `stagePhase` | 0=Ramp, 1=Hold |
| 31 | 1 | — | `_pad` | Выравнивание |

---

## RfidPayload (0x14) — 37 байт

Направление: MCU → LINK.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`RfidEvent`) | `event` | 1=TagDetected, 2=TagRemoved |
| 1 | 1 | uint8 | `readerId` | 0–3 (R1..R4) |
| 2 | 32 | char[32] | `tag` | HEX ID метки, null-terminated; для TagRemoved — пусто |
| 34 | 1 | uint8 | `unitId` | 0–3 |
| 35 | 2 | — | `_pad[2]` | Выравнивание |

---

## CommandPayload (0x20) — 13 байт

Направление: LINK → MCU. **Разделяет kind `0x20` с `ProfilePayload` (64 байта)** — получатель различает формат по `payloadLength`.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`CommandCode`) | `command` | см. таблицу ниже |
| 1 | 1 | uint8 | `targetState` | Для Start: `DryerMode` (1=Drying, 2=Storage, 3=Profile) |
| 2 | 1 | uint8 | `unitId` | 0–3 или `0xFF` = все юниты |
| 3 | 2 | — | `reserved[2]` | Резерв |
| 5 | 4 | uint32 LE | `arg0` | Для Start: target температура × 10 |
| 9 | 4 | uint32 LE | `arg1` | Для Start(Drying): минуты; для Start(Storage): влажность % |

### `CommandCode`

| Код | Имя | Описание |
|-----|-----|----------|
| `0x01` | `Start` | Запуск режима (см. arg0/arg1/targetState) |
| `0x02` | `Stop` | Остановка юнита |
| `0x03` | `Find` | Поиск (мигание) |
| `0x05` | `GetConfig` | Запрос полного JSON конфига |
| `0x06` | `SetConfig` | Применить настройки из JSON |
| `0x07` | `ReadRfid` | Прочитать OpenPrintTag с метки |
| `0x08` | `WriteRfid` | Записать бинарник на метку. Армирует staging на MCU для приёма последующих `RfidWriteData` (0x1B) фрагментов. `arg0 = размер в байтах`, `arg1 = verify-mode` (0=none, 1=header32, 2=full) |
| `0x10` | `ResetFault` | Сброс ошибки юнита |
| `0x11` | `WifiStatus` | Запрос IP от LINK (MCU → LINK) |
| `0x12` | `ClearErrors` | Очистить EEPROM-лог ошибок |

Параметр `0xFF` в `unitId` трактуется как «все юниты» для команд, где это имеет смысл (Stop, ClearErrors).

---

## ProfilePayload (внутри Command 0x20) — 64 байта

Направление: LINK → MCU. Отличается от `CommandPayload` размером payload.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `unitId` | 0–3 |
| 1 | 1 | uint8 | `totalStages` | 1–10 |
| 2 | 1 | uint8 | `startStage` | Индекс старта (0-based) |
| 3 | 1 | — | `_pad` | Выравнивание |
| 4 | 60 | `ProfileStage[10]` | `stages` | До 10 этапов по 6 байт |

### `ProfileStage` (6 байт)

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 2 | uint16 LE | `temp` | Температура × 10 (60 °C → 600) |
| 2 | 2 | uint16 LE | `ramp` | Время разгона, секунды (0 = «форсированный нагрев», см. [../06-flows/04-profile-mode.md](../06-flows/04-profile-mode.md)) |
| 4 | 2 | uint16 LE | `hold` | Время удержания, секунды |

---

## ConfigChunkPayload (0x30) — до 200 байт

Направление: обе стороны. Переносит JSON-фрагмент. Размеры:

- Заголовок `ConfigChunkHeader` = 6 байт.
- Данные `data` = до 194 байт (`MAX_PAYLOAD_SIZE - 6`).

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 2 | uint16 LE | `transferId` | ID передачи (не смешивать разные передачи) |
| 2 | 2 | uint16 LE | `totalSize` | Полный размер JSON (только в первом фрагменте, `chunkIndex == 0`) |
| 4 | 2 | uint16 LE | `chunkIndex` | Индекс фрагмента: 0, 1, … N−1 |
| 6 | ≤194 | bytes | `data` | JSON-данные |

**Фрагментация:**

- Если JSON ≤ 194 байт: один кадр с `FLAG_LAST_FRAGMENT`.
- Если JSON > 194 байт: промежуточные с `FLAG_FRAGMENTED`, последний с `FLAG_LAST_FRAGMENT`.

Подробно: [05-ack-retry.md](05-ack-retry.md).

**Форматы JSON в `data`:**

- MCU → LINK, полный конфиг: `{"v":8,"units":3,"active":0,"lang":"en","menu":[…]}`
- MCU → LINK, delta: `{"d":{"3":[55,60,55],"81":3}}`
- LINK → MCU, set: `{"cmd":"set","id":3,"unit":0,"val":55}`
- LINK → MCU, invoke: `{"cmd":"invoke","id":5}`

---

## HeartbeatPayload (0x40) — 9 байт

Направление: обе стороны.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 4 | uint32 LE | `uptimeSeconds` | Uptime отправителя, секунды |
| 4 | 2 | int16 LE | `wifiRssiDbm` | См. пояснение ниже |
| 6 | 2 | uint16 LE | `errorsSinceBoot` | Счётчик ошибок отправителя (семантика — у отправителя) |
| 8 | 1 | uint8 (`LinkCloudState`) | `cloudState` | Состояние облака — значимо **только в LINK→MCU** |

**`wifiRssiDbm` в обоих направлениях:**

- **LINK → MCU:** RSSI WiFi в dBm. Типично отрицательное (−40…−90). MCU использует значение для индикации силы сигнала в меню/на экране.
- **MCU → LINK:** поле **перегружено**: LINK не имеет своего WiFi-полезного числа от MCU, поэтому референсная прошивка MCU пишет туда **температуру MCU × 10** (int16, положительное число в градусах × 10). LINK это значение нигде в MQTT не пробрасывает — использует только если захочет для своих логов.

!!! warning "Контрактная неоднозначность"
    Такое «двойное назначение» поля — историческое и не идеальное. Если вы делаете свой MCU и вам нужно явно передавать температуру контроллера в LINK — полагайтесь на это только для отладки. Стабильный путь — публиковать свои показатели через `Log` (0x60) или через свои поля в `info`-JSON.

### `LinkCloudState`

| Значение | Имя |
|----------|-----|
| `0` | `Idle` |
| `1` | `WifiConnecting` |
| `2` | `Provisioning` |
| `3` | `Registering` |
| `4` | `AwaitingClaim` |
| `5` | `Ready` |
| `6` | `MqttConnecting` |
| `7` | `Online` |

!!! note "Про `errorsSinceBoot` и логи"
    Семантика `errorsSinceBoot` задаётся **отправителем** и не обязательно едина между LINK и MCU. Референсная прошивка LINK пишет туда счётчик сбоев UART-линка (ACK с ошибкой, приём кадра `Error`), а не счётчик ошибок приложения MCU. Значение из входящего Heartbeat в MQTT **не** пробрасывается.
    Для мониторинга прикладных ошибок используйте кадр `Log` (0x60) и топик MQTT `events`.

---

## LogPayload (0x60) — 164 байта

Направление: MCU → LINK. Структурированное событие приложения.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 10 | char[10] | `severity` | `"critical"`, `"error"`, `"warning"`, `"info"` |
| 10 | 20 | char[20] | `source` | `"THERMISTOR"`, `"HEATER"`, `"SHT"`, … |
| 30 | 32 | char[32] | `event` | `"SENSOR_SHORT"`, `"OVER_MAX"`, `"NO_RESPONSE"`, … |
| 62 | 100 | char[100] | `message` | Сообщение для человека |
| 162 | 1 | uint8 | `unitId` | 0–3 |
| 163 | 1 | — | `_pad` | Выравнивание |

Строковые поля — C-строки, хвост после `\0` заполняется нулями.

!!! note "Отправка в MQTT"
    Публикация `Log` в MQTT-топик `events` **не происходит автоматически** в библиотеке. Это делает прикладной код LINK-прошивки через `setLogHandler()` + `MqttClient::publishEvent()`. Референсная прошивка Link так и сделана — см. `IdryerDevice::handleLog`.

---

## AckPayload (0x11 / 0x21 / 0x31) — 2 байта

Направление: обратное относительно подтверждаемого кадра.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `ackSequence` | `SEQ` подтверждаемого кадра |
| 1 | 1 | uint8 (`ErrorCode`) | `status` | `0` = OK |

Обратите внимание: в **заголовке** ACK-кадра поле `sequence` тоже равно номеру подтверждаемого кадра (не своему собственному). Это дублирование облегчает матчинг.

---

## ErrorPayload (0x50) — 4 байта

Направление: обе стороны. Отправляется при ошибке парсера или валидации.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`ErrorCode`) | `code` | см. таблицу ниже |
| 1 | 1 | uint8 | `lastSequence` | SEQ кадра, вызвавшего ошибку |
| 2 | 2 | uint16 LE | `detail` | Доп. информация (ожидаемая/фактическая длина и т.п.) |

### `ErrorCode`

| Код | Имя | Описание |
|-----|-----|----------|
| `0x00` | `None` | OK |
| `0x01` | `CrcMismatch` | CRC не совпал |
| `0x02` | `UnknownMessage` | Неизвестный `MessageKind` |
| `0x03` | `InvalidPayload` | Неверный размер/формат payload |
| `0x04` | `Busy` | Устройство занято |
| `0x05` | `Timeout` | ACK не получен за 700 мс × 3 |
| `0x06` | `SequenceMismatch` | Неожиданный SEQ |

---

## ClaimStatusPayload (0x71) — 18 байт

Направление: LINK → MCU.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`ClaimingStatus`) | `status` | 0=Idle, 1=Provisioning, 2=WaitingClaim, 3=Claimed, 4=Error |
| 1 | 9 | char[9] | `pin` | PIN 8 цифр + `\0`; пусто вне `WaitingClaim` |
| 10 | 4 | uint32 LE | `expiresAt` | Unix timestamp истечения PIN |
| 14 | 4 | uint32 LE | `remainingSeconds` | Остаток до истечения PIN |

---

## ClaimCompletePayload (0x72) — 38 байт

Направление: LINK → MCU.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `success` | 1 = успех, 0 = ошибка/таймаут |
| 1 | 37 | char[37] | `deviceId` | UUID устройства (только для отображения на экране) |

---

## WsEnablePayload (0x73) — 4 байта

Направление: MCU → LINK.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `enable` | 1 = включить, 0 = выключить |
| 1 | 1 | — | `reserved` | — |
| 2 | 2 | uint16 LE | `pin` | PIN 0–9999 (4 цифры, генерирует MCU) |

---

## WsStatusPayload (0x74) — 6 байт

Направление: LINK → MCU.

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`WsState`) | `state` | 0=Disabled, 1=Listening, 2=Connected |
| 1 | 2 | uint16 LE | `pin` | PIN 0–9999 |
| 3 | 1 | uint8 | `pairedCount` | Привязанных клиентов 0–5 |
| 4 | 1 | uint8 | `maxClients` | Максимум (5) |
| 5 | 1 | — | `reserved` | — |

---

## RfidDataPayload (0x1A / 0x1B) — 199 байт

Направление: 0x1A — MCU→LINK; 0x1B — LINK→MCU. Переносит фрагмент данных метки (всего 888 байт / 163 = 6 фрагментов).

| Offset | Size | Type | Поле | Описание |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `readerId` | 0–3 |
| 1 | 1 | uint8 | `unitId` | 0–3 |
| 2 | 32 | char[32] | `tag` | HEX ID метки (для валидации целевой метки) |
| 34 | 163 | bytes | `fragment` | Фрагмент бинарных данных |
| 197 | 2 | — | `_pad[2]` | Выравнивание |

!!! note "Статус"
    Реализовано end-to-end (`commands/read_rfid`, `commands/write_rfid`). Запись использует stop-and-wait ACK flow control — каждый фрагмент с `FLAG_ACK_REQUIRED`. См. [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## Что дальше

- [05-ack-retry.md](05-ack-retry.md) — как работают подтверждения, retry, фрагментация.
- [06-examples.md](06-examples.md) — живые hex-дампы каждого типа кадра.
