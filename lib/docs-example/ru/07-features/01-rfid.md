# RFID: события и данные меток

Работа с RFID-ридерами: события установки/извлечения катушки и чтение/запись бинарных данных на метку.

!!! note "Статус"
    **События** (`tag_detected`/`tag_removed`) — работают, используются в приложении для привязки катушки к камере.
    **Чтение данных метки** (`commands/read_rfid` → UART `RfidReadData` = 0x1A → MQTT `idryer/<serial>/rfid`) — работает end-to-end.
    **Запись данных на метку** (`commands/write_rfid` → UART `Command WriteRfid` 0x08 + фрагменты `RfidWriteData` 0x1B) — работает end-to-end. Использует stop-and-wait ACK flow control (см. ниже).

---

## Типы оборудования

- RFID-ридеры: 0–3 штук. Идентификаторы `R1`…`R4` (в индексах 0–3).
- Один ридер привязан к одной камере (`unit`). Сопоставление задаётся в `HelloPayload.UnitConfig.rfid[]`.

---

## События (`RfidPayload`, 0x14)

### TagDetected

Катушка поставлена на ридер, либо метка присутствовала при включении устройства.

**UART-payload** (`RfidPayload`):

- `event = TagDetected (1)`
- `readerId = 0..3` (R1..R4)
- `tag` = HEX ID метки, null-terminated, до 32 символов
- `unitId` = 0..3 — к какой камере относится

**MQTT JSON** (`idryer/<serial>/rfid`, retained):

```json
{
  "event": "tag_detected",
  "tag": "DEADBEEF12345678",
  "readerId": 0,
  "unitId": "U1",
  "timestamp": "2026-04-19T12:00:00Z"
}
```

### TagRemoved

Катушка снята с ридера.

**UART:** `event = TagRemoved (2)`, `tag = ""` (пустая).

**MQTT:**

```json
{
  "event": "tag_removed",
  "tag": "",
  "readerId": 0,
  "unitId": "U1",
  "timestamp": "2026-04-19T12:00:00Z"
}
```

!!! note "readerId — число, unitId — строка"
    В MQTT публикации `readerId` — это **число** 0–3, а `unitId` — **строка** `"U1"`…`"U4"`. Несимметрично, как и в других топиках. См. [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md).

!!! note "Auto-read после TagDetected не выполняется"
    UID и содержимое метки идут двумя **независимыми** событиями: `RfidPayload` (0x14) с UID и `RfidDataPayload` (0x1A) с бинарным содержимым. Содержимое читается по явной команде `commands/read_rfid`. Это сознательный выбор — `tag_detected` быстрое событие, чтение содержимого может занимать сотни миллисекунд.

---

## Кэширование событий

LINK при подключении к MQTT публикует **текущее состояние каждого ридера** (retained). Это:

- последнее известное событие — `tag_detected` с ID метки, либо `tag_removed` с пустым tag;
- гарантия: приложение, подключившись к топику `rfid`, сразу увидит актуальный статус катушек без ожидания.

На стороне MCU логика:

- при **boot** отправить одно событие на каждый ридер — `TagDetected` с ID или `TagRemoved` с пустым tag;
- в рантайме — по изменениям.

---

## Правило поллинга: только в Idle

Синхронный поллинг PN5180 блокирует main loop RP2040 на десятки миллисекунд. Чтобы не ломать тайминги PID, LED и других реалтайм-задач:

**MCU поллит RFID-ридеры только когда ВСЕ контроллеры камер находятся в `DryerMode::Idle`.**

В режимах `Drying`, `Storage`, `Profile`, `PidAutoTune` фоновый поллинг приостанавливается. Уже запущенные задачи (чтение/запись/preview) продолжают работать до завершения. Явная команда `commands/read_rfid` или `commands/write_rfid` от портала обслуживается в любом режиме — это точечная операция по запросу, а не фоновый детект.

---

## Чтение данных метки

### UART команда

```
MQTT commands/read_rfid  { "unitId": "U1" }
          ↓
CommandHandler → UART Command{ReadRfid, unitId=0}
```

MCU запрашивает у ридера блоки данных метки. Объём данных определяется типом чипа (см. таблицу ниже).

### UART ответ (RfidReadData, 0x1A)

Ответ фрагментируется по 163 байта полезных данных на кадр `RfidDataPayload`. **Количество фрагментов переменное**: `nFrags = ceil(readSize / 163)`, где `readSize` — реальный объём user-memory чипа. LINK определяет конец цепочки по `FLAG_LAST_FRAGMENT`, счётчик не проверяет.

| Чип | Читается | Кол-во фрагментов |
|---|---|---|
| NTAG213 | 144 Б | 1 |
| SLIX2 (ISO15693) | 316 Б | 2 |
| NTAG215 | 504 Б | 4 |
| NTAG216 | 888 Б | 6 |
| MIFARE Classic 1K | 768 Б (48 × 16, включая block 0) | 5 |

!!! note "MIFARE: чтение vs запись"
    Для MIFARE объём **чтения** и **записи** разный. Read отдаёт **768 Б** (48 блоков × 16, включая block 0 с UID/manufacturer data — он read-only, но читается свободно). Write может изменить только **752 Б** (47 user-блоков × 16, без block 0 и sector trailer-блоков). См. раздел [MIFARE Classic 1K](#mifare-classic-1k) ниже.

Флаги:

- промежуточные фрагменты — `FLAG_FRAGMENTED`,
- последний — `FLAG_LAST_FRAGMENT`.

**LINK собирает ответ по флагу `FLAG_LAST_FRAGMENT`, не по счётчику.**

Все кадры несут `readerId`, `unitId`, `tag` (для валидации, что данные действительно с этой метки).

!!! note "Почему 163, а не 194 как у ConfigPush?"
    `ConfigChunkPayload` (0x30) — чистый JSON с 6-байтным заголовком чанка: `200 − 6 = 194` байта полезных данных на кадр.

    `RfidDataPayload` (0x1A/0x1B) несёт **фиксированные мета-поля** (readerId, unitId, tag\[32\]) + `_pad[2]`, итого служебных 36 байт. Остаётся `199 − 36 = 163` байта для самих данных метки. Отсюда и другой размер фрагмента.

Структура — [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

### MQTT публикация

LINK после сборки всех фрагментов кладёт base64-кодированный буфер в `idryer/<serial>/rfid` (retained):

```json
{
  "readerId": 0,
  "unitId": "U1",
  "tagId": "04:C1:42:A1:74:26:81",
  "format": "unknown",
  "data": "A/8BL8IcAAAB..."
}
```

Поле `format` — подсказка порталу о типе содержимого. Значения: `"empty"` (все байты = 0, `data = null`), `"openprinttag"` (найдена MIME-сигнатура Prusa OpenPrintTag), `"openspool"` (найдена MIME-сигнатура OpenSpool), `"unknown"` (сигнатура не распознана). Детекция — в первых 128 байтах буфера. Полный парсинг содержимого делает портал.

---

## Форматы данных метки

MCU отдаёт сырой бинарник — интерпретация формата на портале.

| Чип | Типичный формат | Примечание |
|---|---|---|
| SLIX2 (ISO15693) | OpenPrintTag (Prusa, CBOR + NDEF) | ~316 Б |
| NTAG215 | OpenSpool (JSON внутри NDEF) | до 504 Б |
| NTAG216 | OpenSpool (JSON) | до 888 Б |
| MIFARE Classic 1K | нет open-стандарта | зависит от кейса |

OpenPrintTag начинается с NDEF TLV: `03 FF LL LL C2 1C 00 00 01 0D 61 70 70 6C 69 63 61 74 69 6F 6E 2F 76 6E 64 2E 6F 70 65 6E 70 72 69 6E 74 74 61 67 …` (MIME `application/vnd.openprinttag`).

---

## Запись данных на метку

### MQTT команда

```json
MQTT commands/write_rfid
{
  "unitId": "U1",
  "data": "BASE64_STRING...",
  "verify": "header32"
}
```

Параметры:

- `unitId` — идентификатор камеры `"U1"`…`"U4"`.
- `data` — base64-строка, до 888 байт после декодирования.
- `verify` — режим сверки после записи: `"none"` (без сверки), `"header32"` (первые 32 байта, по умолчанию), `"full"` (весь буфер). Опциональное; если поле отсутствует — `"header32"`.

### Цепочка UART

LINK декодирует base64 и отправляет на MCU:

1. `Command WriteRfid` (0x08): `arg0 = rawLen`, `arg1 = verifyCode` (0/1/2).
2. Фрагменты `RfidWriteData` (0x1B) по 163 байта полезных данных на кадр. Промежуточные — `FLAG_FRAGMENTED`, последний — `FLAG_LAST_FRAGMENT`.

Количество фрагментов — `ceil(rawLen / 163)`.

### Stop-and-wait ACK flow control

**Каждый кадр в цепочке WriteRfid идёт с `FLAG_ACK_REQUIRED`. LINK не шлёт следующий кадр, пока не получит ACK на текущий.**

Схема основана на XMODEM stop-and-wait ARQ и CAN-TP (ISO 15765-2 §9–10) с параметрами BlockSize / STmin. Нужна потому, что приёмник (RP2040) параллельно крутит PID, опрашивает весы, при необходимости поллит PN5180. Hardware-FIFO UART на RP2040 = 32 байта (≈2.7 мс при 115200), переполняется, если ISR задерживается. Back-to-back 199-байтные фрагменты без паузы между собой гарантированно теряют байты → CRC mismatch.

ACK-pacing:

- `LINK → MCU`: `Command WriteRfid` с `FLAG_ACK_REQUIRED`.
- `MCU → LINK`: `CommandAck` со `status = None` (staging armed) или `InvalidPayload` (нет ридера/метки/переполнение размера).
- `LINK`: блокируется в `UartBridge::waitForAck(timeoutMs = 200)`, внутри крутит `loop()` для обработки RX и retry.
- При успехе — шлёт первый фрагмент с `FLAG_ACK_REQUIRED | FLAG_FRAGMENTED`, ждёт ACK. И так до последнего.
- При таймауте — обрывает транзакцию и логирует в MQTT events.

### MCU side

1. При приёме `Command WriteRfid` в UART-handler **синхронно** армирует staging-буфер через `rfidArmWriteStagingByUnit(unitId, rawLen, verifyCode)`. Это необходимо, потому что LINK шлёт все кадры без пауз — enqueue-через-main-loop создаст окно, где первый фрагмент придёт до armed staging и будет отброшен.
2. Параллельно выставляется sync-флаг `g_rfidWritePending[reader]`, блокирующий PN5180-поллинг этого ридера до завершения транзакции.
3. `handleUartRfidData` копирует каждый фрагмент в staging и **отправляет ACK** через `sendCommandAck(sequence, status)`.
4. На последнем фрагменте (`FLAG_LAST_FRAGMENT`) запускается `rfidStartWriteTagData(reader, buffer, len, verifyMode)`. ACK с `status = None` уходит после успешного запуска write-job, `InvalidPayload` — при несовпадении полученного размера.

### Ограничения

- Размер `data` (после base64-decode) не должен превышать writable-объём чипа. Для NTAG213 — 144 Б, для SLIX2 — ~316 Б, для NTAG215 — 504 Б, для NTAG216 — 888 Б, для MIFARE Classic 1K — **752 Б** (47 user-блоков × 16; block 0 и sector trailer-блоки не пишутся).
- Метка должна находиться на ридере **на момент отправки `write_rfid` и во время выполнения записи**. Если метка снимается в середине, драйвер PN5180 вернёт ошибку, staging-буфер очистится.
- `ACK timeout = 200 мс` и `MAX_RETRIES = 3` задаются в `handleWriteRfid` — при превышении транзакция обрывается.

---

## MIFARE Classic 1K

MIFARE Classic 1K **читается и пишется** с дефолтным ключом Key A = `FF FF FF FF FF FF`. Ограничение: **метки Bambu** (MIFARE с RSA-подписью производителя) доступны только на чтение — запись невозможна без приватного ключа.

Open-стандарта формата под MIFARE 1K на момент написания нет. Приложение само решает, как разместить данные.

### Размеры и layout

Ридер обходит сектора 0..15 по очереди, аутентифицируясь Key A на sector trailer. Сектор 0 содержит manufacturer block (block 0), где лежит UID и данные производителя — этот блок **read-only**, write-операция туда физически невозможна. Sector trailer-блоки (3, 7, 11, …, 63) хранят ключи и access bits — в open-протоколе мы их тоже не трогаем, чтобы не уронить метку.

| Операция | Объём | Блоки |
|---|---|---|
| Read | 768 Б | 0, 1, 2, 4, 5, 6, 8, …, 62 (48 блоков, trailer'ы пропущены) |
| Write | 752 Б | 1, 2, 4, 5, 6, 8, …, 62 (47 блоков, block 0 и trailer'ы пропущены) |

Маппинг `userBlockIdx → pageNo` для MQTT-буфера:

- **Read:** `idx ∈ [0..47]` → `pageNo = (idx / 3) * 4 + (idx % 3)` → 0, 1, 2, 4, 5, 6, 8, …
- **Write/verify:** `idx ∈ [0..46]` → первые два блока `idx + 1`, дальше `4 + (idx−2)/3 * 4 + (idx−2) % 3` → 1, 2, 4, 5, 6, 8, …

Асимметрия объёма (768 на чтение, 752 на запись) — следствие того, что block 0 читается, но не пишется.

### Надёжность: retry на sector switch

При пересечении границы сектора драйвер делает `fieldOff → delay → re-SELECT → mifareAuthenticate`. На реальном железе auth этого нового сектора может транзиентно фейлиться (тег NAK'ает auth-запрос с `authStatus = 1`) — следствие PN5180 timing'а между выключением RF-поля и повторным SELECT.

PN5180-драйвер MCU ретраит auth до **3 раз** с полным циклом `fieldOff → delay (5 мс на первой попытке, 15 мс на retry) → re-SELECT → mifareAuthenticate`. Это спасает чтение/запись от случайного фейла на сторонах сектора без потери данных.

---

## Особенности на MCU

- `ReadRfid` возвращает `CommandAck` немедленно (команда принята), сами данные уходят отдельными кадрами `RfidReadData`.
- Если метки нет, MCU отправляет `CommandAck` со `status = InvalidPayload` — LINK увидит ошибку в `events`.
- Если MCU не поддерживает RFID (`unit.rfid[] = 0xFF` в Hello), LINK не передаёт команду в UART и логирует ошибку.

---

## Что дальше

- [02-websocket-local.md](02-websocket-local.md) — локальный WebSocket без облака.
- [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md) — RFID в общем списке JSON-публикаций.
- [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md) — `RfidPayload` / `RfidDataPayload`.
- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) — `commands/read_rfid` / `commands/write_rfid`.
