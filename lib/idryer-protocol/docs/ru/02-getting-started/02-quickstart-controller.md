# Quickstart: свой контроллер (MCU)

Минимальный путь от «пустой IDE» до «первый кадр ушёл в модуль LINK и я получил ответ». Ничего лишнего: чистый Arduino, один сетап, никаких PlatformIO и внешних библиотек. После квикстарта вы понимаете протокол насквозь и легко переносите код на любую платформу.

!!! note "Что здесь и чего здесь нет"
    Здесь — **ручная сборка UART-кадра**: из байтов, с CRC, без библиотеки. Это важно, чтобы протокол не был для вас «чёрным ящиком».
    После квикстарта, если хочется компактнее, можно подключить `idryer-protocol` как заголовки (структуры + `calculateCrc()` уже готовые) — см. последний раздел.

!!! warning "Базовые концепты — читайте prerequisites"
    Дальше используются: фиксированные типы, битовые операции, little-endian, CRC, `enum class`. Если хотя бы один термин звучит непонятно — сначала пройдите [../01-overview/05-prerequisites.md](../01-overview/05-prerequisites.md) (10 минут). Без базы квикстарт превращается в шифр.

---

## Цель квикстарта

Ваш контроллер (MCU) делает два действия:

1. Отправляет кадр `Hello` модулю LINK.
2. Раз в секунду шлёт кадр `Telemetry` с температурой, влажностью и мощностью нагревателя.

На выходе — реальный поток байтов в UART, который LINK примет и опубликует в MQTT.

---

## Что понадобится

**Железо:**

- Любая Arduino-совместимая плата **c двумя аппаратными UART** (ESP32, RP2040 Pico, STM32 Blue Pill, Arduino Mega и т.д.). Один UART — для дебага через USB, второй — для связи с LINK.
- Модуль LINK (готовая плата iDryer Link). **Или** для теста без LINK — USB-UART адаптер и компьютер с любым бинарным терминалом (Hercules, CoolTerm, `socat`).

!!! warning "Uno / Nano / ATmega328 — не подходят"
    У классических Arduino Uno/Nano только **один** `Serial`, и он занят USB-монитором. Для UART к LINK нужен второй аппаратный порт. `SoftwareSerial` на 115200 бод работает нестабильно — не пытайтесь.

    **Берите**: Mega 2560, ESP32 (любой вариант), RP2040 Pico, STM32 Blue Pill, Teensy. У всех есть `Serial` **и** `Serial1`.

**Пины `Serial1` на популярных платах:**

| Плата | `Serial1` TX | `Serial1` RX | Примечание |
|-------|--------------|--------------|------------|
| Arduino Mega 2560 | 18 | 19 | «из коробки» |
| RP2040 Pico (Arduino core) | любой | любой | `Serial1.setTX(0); Serial1.setRX(1);` до `Serial1.begin(...)` |
| STM32 Blue Pill | PA9 | PA10 | в Arduino core `Serial1` маплен по умолчанию |
| ESP32 | любой | любой | `Serial1.begin(115200, SERIAL_8N1, RX, TX)` |

Скетч ниже по умолчанию берёт `Serial1`. При необходимости подправьте инициализацию под вашу плату.

**Софт:**

- Arduino IDE или любой редактор с `avr-gcc` / `arm-gcc`.
- Опционально Python 3 — для разбора байт в терминале.

**Подключение (UART MCU ↔ LINK):**

```
   MCU                LINK (ESP32)
  ┌───┐              ┌───┐
  │TX ├──────────────┤RX │
  │RX ├──────────────┤TX │
  │GND├──────────────┤GND│
  └───┘              └───┘
```

Параметры: **115200 бод, 8N1, без аппаратного контроля потока**. Никаких резисторов и подтяжек — уровни 3.3 В у ESP32; если ваш MCU пятивольтовый (классический Arduino Uno), нужен level-shifter.

!!! warning "Питание"
    Никогда не подключайте и не отключайте кабели UART при поданном питании. Сначала отключите питание обеих плат.

---

## Устройство UART-кадра (короткий обзор)

Прежде чем писать код, поймите структуру кадра. Без этого `0xAA` в логе будет загадкой.

```
┌─────┬─────┬──────┬──────┬─────┬─────┬─────────────┬───────────┐
│ SOF │ VER │FLAGS │ KIND │ SEQ │ LEN │   PAYLOAD   │   CRC16   │
│ 1B  │ 1B  │  1B  │  1B  │ 1B  │ 1B  │ 0–200 байт  │  2B (LE)  │
└─────┴─────┴──────┴──────┴─────┴─────┴─────────────┴───────────┘
```

| Поле | Размер | Значение |
|------|--------|----------|
| SOF | 1 байт | Всегда `0xAA` — маркер начала кадра |
| VER | 1 байт | Версия протокола, всегда `0x01` |
| FLAGS | 1 байт | Битовые флаги (ACK, ошибка, фрагментация) |
| KIND | 1 байт | Тип сообщения (`0x01` = Hello, `0x10` = Telemetry, и т.д.) |
| SEQ | 1 байт | Счётчик 0–255, инкремент при каждом кадре |
| LEN | 1 байт | Длина payload, 0–200 |
| PAYLOAD | LEN байт | Данные, структура зависит от KIND |
| CRC | 2 байта | CRC16-CCITT от `[SOF … конец PAYLOAD]`, записан little-endian |

**Все многобайтовые числа в payload — little-endian.** Это важно. `uint32_t = 0x00010000` (версия прошивки 1.0.0) пишется как `00 00 01 00`.

**CRC считается от всего, что перед ним** — от SOF включительно до последнего байта payload. Не включая сам CRC.

Подробно все поля и структуры: [../03-uart/](../03-uart/). Здесь — только то, что нужно сейчас.

---

## Кто говорит первым

Рукопожатие между MCU и LINK устроено симметрично — оба могут инициировать.

**Сценарий A — MCU стартовал первым:**

1. MCU шлёт `Hello` с role=`0x01` (MCU).
2. Если LINK уже готов, отвечает `HelloAck` с IP/SSID.
3. Если нет — кадр может уйти «в пустоту»; MCU должен повторять `Hello`, пока не получит ответ.

**Сценарий B — LINK стартовал первым:**

1. LINK шлёт пустой `Hello` с role=`0xFF` (`HelloRequest`) — это триггер: «эй MCU, пришли мне свой Hello».
2. MCU видит `Hello` с role=`0xFF` и **в ответ** шлёт свой полный `Hello`.
3. LINK отвечает `HelloAck`.

**Что должен уметь ваш MCU:**

- при загрузке шлёт свой `Hello` один раз;
- параллельно слушает входящие кадры; если приходит `Hello` с role=`0xFF` — снова шлёт свой полный `Hello`;
- опционально: до первого `HelloAck` повторять Hello каждые 5 секунд (LINK переживает boot/переподключение WiFi — может долго не отвечать).

Мини-скетч из следующего шага делает только первый шаг — один `Hello` в `setup()`. Для продакшена добавьте retry и реакцию на `Role::HelloRequest` (`0xFF`).

---

## Шаг 1. Реализация CRC16-CCITT

Один раз пишете и забываете. Полином `0x1021`, начальное значение `0xFFFF`.

```cpp
uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

Идентична референсной реализации в библиотеке (`src/uart/uart_protocol.cpp`).

---

## Шаг 2. Минимальный скетч

Полный работающий пример для Arduino. Отправляет Hello при старте, раз в секунду — Telemetry.

```cpp
#include <Arduino.h>
#include <string.h>

// ============================================================================
// Константы UART-протокола iDryer
// ============================================================================
constexpr uint8_t SOF = 0xAA;
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t MAX_PAYLOAD = 200;

// Типы сообщений (MessageKind)
constexpr uint8_t KIND_HELLO     = 0x01;
constexpr uint8_t KIND_HELLO_ACK = 0x02;
constexpr uint8_t KIND_TELEMETRY = 0x10;

// Role
constexpr uint8_t ROLE_MCU = 0x01;

// DeviceType
constexpr uint8_t DEVICE_DRYER = 0x01;

// Capabilities bits
constexpr uint16_t CAP_HEATER     = (1 << 0);
constexpr uint16_t CAP_FAN        = (1 << 1);
constexpr uint16_t CAP_RH_AIR     = (1 << 3);
constexpr uint16_t CAP_TEMP_AIR   = (1 << 4);

// ============================================================================
// CRC16-CCITT
// ============================================================================
uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

// ============================================================================
// Отправка кадра
// ============================================================================
uint8_t seqCounter = 0;

void sendFrame(uint8_t kind, const uint8_t *payload, uint8_t payloadLen, uint8_t flags = 0) {
    uint8_t buf[6 + MAX_PAYLOAD + 2];

    buf[0] = SOF;
    buf[1] = PROTOCOL_VERSION;
    buf[2] = flags;
    buf[3] = kind;
    buf[4] = seqCounter++;
    buf[5] = payloadLen;
    memcpy(&buf[6], payload, payloadLen);

    uint16_t crc = crc16(buf, 6 + payloadLen);
    buf[6 + payloadLen]     = crc & 0xFF;         // CRC low byte first (LE)
    buf[6 + payloadLen + 1] = (crc >> 8) & 0xFF;

    Serial1.write(buf, 6 + payloadLen + 2);
}

// ============================================================================
// Сборка Hello payload (86 байт)
// Структура HelloPayload — см. docs/03-uart/04-binary-structures.md
// ============================================================================
void sendHello() {
    uint8_t payload[86] = {0};
    size_t i = 0;

    payload[i++] = ROLE_MCU;         // role
    payload[i++] = DEVICE_DRYER;     // deviceType
    payload[i++] = 0;                // _pad1[0]
    payload[i++] = 0;                // _pad1[1]

    // firmwareVersion = 1.0.0 = 0x00010000, LE
    uint32_t fw = ((uint32_t)1 << 16) | (0 << 8) | 0;
    memcpy(&payload[i], &fw, 4);  i += 4;

    uint32_t workTime = 0;           // workTimeCounter
    memcpy(&payload[i], &workTime, 4); i += 4;

    // hardwareVersion[8] = "v1.0"
    strncpy((char*)&payload[i], "v1.0", 8);
    i += 8;

    payload[i++] = 1;                // unitsCount = 1

    // units[0]: unitId=0, caps=HEATER|FAN|RH|TEMP_AIR, scales и rfid = 0xFF (не используются)
    uint16_t caps = CAP_HEATER | CAP_FAN | CAP_RH_AIR | CAP_TEMP_AIR;
    payload[i++] = 0;                // unitId
    payload[i++] = 0;                // _pad1
    memcpy(&payload[i], &caps, 2); i += 2;
    memset(&payload[i], 0xFF, 4);  i += 4;   // scales
    memset(&payload[i], 0xFF, 4);  i += 4;   // rfid
    // units[1..3] — остаются нулями (unitsCount=1)
    constexpr size_t UNIT_CONFIG_SIZE = 12;
    i += UNIT_CONFIG_SIZE * 3;

    // mcuSerial[17] — 16 hex + '\0'
    strncpy((char*)&payload[i], "36B955AB4350FEDC", 17);
    // i += 17;   // итого i должен быть 86

    sendFrame(KIND_HELLO, payload, sizeof(payload));
}

// ============================================================================
// Сборка Telemetry payload
// Вариант: одна камера, вторую часть массива заполняем нулями
// ============================================================================
void sendTelemetry(float tempC, float humidityPct, uint8_t heaterPct, bool fanOn) {
    uint8_t payload[29] = {0};  // count(1) + 4*TelemetryEntry(7) = 29
    payload[0] = 1;              // count = одна камера заполнена

    int16_t  t10 = (int16_t)(tempC * 10.0f);
    uint16_t h10 = (uint16_t)(humidityPct * 10.0f);

    // TelemetryEntry[0] — 7 байт
    payload[1] = 0;                              // unitId = 0
    memcpy(&payload[2], &t10, 2);                // temperatureC10 (LE)
    memcpy(&payload[4], &h10, 2);                // humidityPct10 (LE)
    payload[6] = heaterPct;                      // heaterPowerPct
    payload[7] = fanOn ? 1 : 0;                  // fanOn

    // Entries 1..3 — нулевые. LINK увидит count=1 и прочитает только первый.

    sendFrame(KIND_TELEMETRY, payload, sizeof(payload));
}

// ============================================================================
// Setup & loop
// ============================================================================
uint32_t lastTelemetry = 0;

void setup() {
    Serial.begin(115200);           // дебаг-порт
    Serial1.begin(115200);           // к LINK (зависит от платы: pin mapping может отличаться)

    delay(500);
    Serial.println("MCU start, sending Hello...");
    sendHello();
}

void loop() {
    uint32_t now = millis();
    if (now - lastTelemetry >= 1000) {
        lastTelemetry = now;
        sendTelemetry(55.3f, 45.2f, 80, true);
        Serial.println("Telemetry sent");
    }

    // Входящие кадры — упрощённо, читаем и печатаем hex (полноценный парсер — в Шаге 4)
    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        Serial.print(b, HEX); Serial.print(' ');
    }
}
```

Заливаете, открываете Serial Monitor на 115200. В мониторе увидите `MCU start, sending Hello...`, затем раз в секунду — `Telemetry sent` и (если подключён реальный LINK) — входящие байты от него.

---

## Шаг 3. Как выглядит кадр на проводе

### Layout HelloPayload (86 байт)

Прежде чем смотреть hex-дамп, полезно увидеть разбивку полей payload. Суммарно — ровно 86 байт:

| Offset | Size | Поле | Описание |
|--------|------|------|----------|
| 0 | 1 | `role` | 0x01 = MCU |
| 1 | 1 | `deviceType` | 0x01 = Dryer |
| 2 | 2 | `_pad1[2]` | выравнивание |
| 4 | 4 | `firmwareVersion` | uint32 LE: `(MAJOR<<16)\|(MINOR<<8)\|PATCH` |
| 8 | 4 | `workTimeCounter` | uint32 LE: наработка в секундах |
| 12 | 8 | `hardwareVersion[8]` | ASCII `"v1.0\0\0\0\0"` |
| 20 | 1 | `unitsCount` | 0–4 |
| 21 | 48 | `units[4]` | 4 × `UnitConfig` по 12 байт |
| 69 | 17 | `mcuSerial[17]` | 16 hex + `\0` |

**Итого:** 1+1+2+4+4+8+1+48+17 = **86**.

**`UnitConfig`** (12 байт):

| Offset | Size | Поле |
|--------|------|------|
| 0 | 1 | `unitId` (0–3) |
| 1 | 1 | `_pad1` |
| 2 | 2 | `capabilities` (uint16 LE, битовые флаги) |
| 4 | 4 | `scales[4]` (индексы датчиков весов, `0xFF` = нет) |
| 8 | 4 | `rfid[4]`   (индексы RFID-ридеров, `0xFF` = нет) |

Неиспользованные `units[1..3]` в вашем скетче — 36 нулевых байт (они обязательны для фиксированной длины 86 байт; LINK их не читает, потому что `unitsCount = 1`).

### Полный hex-дамп Hello

Вы отправляете `Hello` с параметрами из скетча. Вот **полный байтовый дамп** того, что уходит в UART:

```
Offset       Hex bytes                                              Описание
──────────── ─────────────────────────────────────────────────────  ─────────────────────
0x00         AA                                                     SOF
0x01         01                                                     VER = 1
0x02         00                                                     FLAGS = 0
0x03         01                                                     KIND = Hello
0x04         00                                                     SEQ = 0
0x05         56                                                     LEN = 86

--- payload (86 байт) ---
0x06         01                                                     role = MCU
0x07         01                                                     deviceType = Dryer
0x08..0x09   00 00                                                  _pad1[2]
0x0A..0x0D   00 00 01 00                                            firmwareVersion = 0x00010000 (1.0.0 LE)
0x0E..0x11   00 00 00 00                                            workTimeCounter = 0
0x12..0x19   76 31 2E 30 00 00 00 00                                hardwareVersion = "v1.0\0\0\0\0"
0x1A         01                                                     unitsCount = 1
0x1B..0x1C   00 00                                                  unit[0]: unitId=0, _pad1=0
0x1D..0x1E   1B 00                                                  unit[0]: capabilities=0x001B (LE)
0x1F..0x22   FF FF FF FF                                            unit[0]: scales[4] = all 0xFF (unused)
0x23..0x26   FF FF FF FF                                            unit[0]: rfid[4]   = all 0xFF (unused)
0x27..0x4A   (36 байт нулей)                                        unit[1..3] — пустые (unitsCount=1)
0x4B..0x5B   33 36 42 39 35 35 41 42 34 33 35 30 46 45 44 43 00     mcuSerial "36B955AB4350FEDC\0"

--- CRC (2 байта, little-endian) ---
0x5C..0x5D   74 E2                                                  CRC16 = 0xE274 → low first
```

Итого 94 байта. В терминале непрерывным потоком:

```
AA 01 00 01 00 56 01 01 00 00 00 00 01 00 00 00 00 00 76 31
2E 30 00 00 00 00 01 00 00 1B 00 FF FF FF FF FF FF FF FF 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 33 36 42 39
35 35 41 42 34 33 35 30 46 45 44 43 00 74 E2
```

### Для Telemetry выглядит так (37 байт):

```
AA 01 00 10 00 1D 01 00 29 02 C4 01 50 01 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 DA
```

Разбор:

```
Offset       Hex bytes                              Описание
──────────── ──────────────────────────────────────  ──────────────────────────
0x00         AA                                     SOF
0x01         01                                     VER = 1
0x02         00                                     FLAGS = 0
0x03         10                                     KIND = Telemetry
0x04         00                                     SEQ = 0
0x05         1D                                     LEN = 29

--- payload (29 байт) ---
0x06         01                                     count = 1
0x07         00                                     entry[0].unitId = 0
0x08..0x09   29 02                                  temperatureC10 = 0x0229 = 553 → 55.3 °C
0x0A..0x0B   C4 01                                  humidityPct10  = 0x01C4 = 452 → 45.2 %
0x0C         50                                     heaterPowerPct = 80
0x0D         01                                     fanOn = 1
0x0E..0x22   (21 байт нулей)                        entry[1..3] не используются (count=1)

--- CRC (little-endian) ---
0x23..0x24   10 DA                                  CRC16 = 0xDA10
```

Вы можете **руками проверить CRC** через готовый онлайн-калькулятор. Выбирайте вариант **CRC-16/CCITT-FALSE** (он же **CRC-16/IBM-3740**): polynomial `0x1021`, init `0xFFFF`, RefIn/RefOut = false, XorOut = `0x0000`.

!!! warning "Не путайте с XMODEM"
    В онлайн-калькуляторах часто рядом стоят варианты: **CCITT-FALSE (init 0xFFFF)** и **XMODEM (init 0x0000)**. У iDryer — первый. Если возьмёте XMODEM, получите другое значение и будете подозревать свой код; проверьте сначала init-значение.

Скормите первые 35 байт кадра Telemetry (без последних двух). Должно получиться `0xDA10`. Для Hello скормите первые 92 байта — получится `0xE274`.

**Проверка без железа (Python):**

```python
def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

tele = bytes.fromhex("AA 01 00 10 00 1D 01 00 29 02 C4 01 50 01 "
                     "00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
                     "00 00 00 00 00 00 00".replace(" ", ""))
print(hex(crc16_ccitt_false(tele)))   # -> 0xda10
```

Эта функция 1-в-1 повторяет C-реализацию из библиотеки (`src/uart/uart_protocol.cpp`).

---

## Шаг 4. Приём HelloAck

LINK ответит кадром `HelloAck` (KIND=`0x02`, LEN=37). Чтобы распарсить его, нужен мини-парсер. Добавьте в скетч:

```cpp
// Индексы полей в заголовке (чтобы не писать магические hdr[3], hdr[5])
enum HeaderField {
    HDR_SOF   = 0,
    HDR_VER   = 1,
    HDR_FLAGS = 2,
    HDR_KIND  = 3,
    HDR_SEQ   = 4,
    HDR_LEN   = 5,
    HDR_SIZE  = 6,
};

enum class ParserState : uint8_t {
    WaitSof, Header, Payload, Crc
};

static ParserState parser_state = ParserState::WaitSof;
static uint8_t    hdr[HDR_SIZE];
static uint8_t    hdrIdx = 0;
static uint8_t    pl[MAX_PAYLOAD];
static uint8_t    plIdx = 0;
static uint16_t   rxCrc = 0;
static uint8_t    crcIdx = 0;

// Декларация handleFrame — реализация ниже
void handleFrame(uint8_t kind, const uint8_t *payload, uint8_t len);

// Переход в состояние Crc с обнулением rxCrc — общий для обоих путей
// (LEN=0 сразу из Header, LEN>0 из Payload)
static inline void gotoCrcState() {
    parser_state = ParserState::Crc;
    crcIdx = 0;
    rxCrc  = 0;
}

void parseByte(uint8_t b) {
    switch (parser_state) {
    case ParserState::WaitSof:
        if (b == SOF) {
            hdr[HDR_SOF] = b;
            hdrIdx = 1;
            parser_state = ParserState::Header;
        }
        break;

    case ParserState::Header:
        hdr[hdrIdx++] = b;
        if (hdrIdx == HDR_SIZE) {
            // Базовая валидация: версия и длина payload
            if (hdr[HDR_VER] != PROTOCOL_VERSION || hdr[HDR_LEN] > MAX_PAYLOAD) {
                parser_state = ParserState::WaitSof;
                return;
            }
            plIdx = 0;
            if (hdr[HDR_LEN] == 0) {
                gotoCrcState();          // кадр без payload (например, HelloRequest)
            } else {
                parser_state = ParserState::Payload;
            }
        }
        break;

    case ParserState::Payload:
        pl[plIdx++] = b;
        if (plIdx == hdr[HDR_LEN]) {
            gotoCrcState();
        }
        break;

    case ParserState::Crc:
        if (crcIdx == 0) {
            rxCrc   = b;
            crcIdx  = 1;
        } else {
            rxCrc  |= (uint16_t)b << 8;

            // Склеиваем header + payload и считаем ожидаемый CRC
            uint8_t buf[HDR_SIZE + MAX_PAYLOAD];
            memcpy(buf, hdr, HDR_SIZE);
            memcpy(buf + HDR_SIZE, pl, hdr[HDR_LEN]);
            uint16_t computed = crc16(buf, HDR_SIZE + hdr[HDR_LEN]);

            if (computed == rxCrc) {
                handleFrame(hdr[HDR_KIND], pl, hdr[HDR_LEN]);
            } else {
                Serial.print("[RX] CRC mismatch: got 0x");
                Serial.print(rxCrc, HEX);
                Serial.print(", expected 0x");
                Serial.println(computed, HEX);
            }
            parser_state = ParserState::WaitSof;
        }
        break;
    }
}

void handleFrame(uint8_t kind, const uint8_t *payload, uint8_t len) {
    if (kind == KIND_HELLO_ACK && len == 37) {
        // HelloAckPayload: ipAddress (uint32 LE) + ssid[33]
        uint32_t ip;
        memcpy(&ip, payload, 4);
        char ssid[34];
        memcpy(ssid, payload + 4, 33);
        ssid[33] = '\0';

        Serial.print("[HelloAck] IP=");
        Serial.print(ip & 0xFF);         Serial.print('.');
        Serial.print((ip >> 8)  & 0xFF); Serial.print('.');
        Serial.print((ip >> 16) & 0xFF); Serial.print('.');
        Serial.print((ip >> 24) & 0xFF);
        Serial.print(" SSID=");
        Serial.println(ssid);
    } else {
        Serial.print("[RX] Frame kind=0x");
        Serial.print(kind, HEX);
        Serial.print(" len=");
        Serial.println(len);
    }
}
```

Замените в `loop()` блок «входящие кадры» на:

```cpp
while (Serial1.available()) {
    parseByte(Serial1.read());
}
```

При успехе в Serial Monitor увидите:

```
[HelloAck] IP=192.168.1.5 SSID=MyHomeNetwork
```

---

## Если что-то не работает

**Кадр не уходит в LINK (LINK молчит):**

- проверьте кроссировку: `MCU TX → LINK RX`, `MCU RX ← LINK TX`, общая `GND`;
- проверьте уровни напряжения — если MCU 5 В, нужен level-shifter к 3.3 В ESP32;
- замерьте осциллографом или логическим анализатором на 115200 бод — должен быть виден байт `0xAA` в начале.

**Приходит мусор в MCU (случайные байты):**

- разные baud rate с двух сторон. Убедитесь, что LINK тоже на 115200.
- плохой контакт GND — добавьте общую землю.

**CRC не сходится:**

- проверьте, что считаете CRC **от всех** байт заголовка + payload, **не включая** сам CRC.
- проверьте init: `0xFFFF`, не `0x0000` (частая ошибка).
- подайте эталонный массив из Telemetry-дампа (35 байт) и сверьте с `0xDA10`.

**Парсер зацикливается на случайном байте `0xAA` внутри payload:**

- такое возможно и нормально — парсер начнёт собирать кадр с ложного SOF, но после проверки CRC отбросит и вернётся в `WaitSof`. Никаких дополнительных действий не требуется.

**Парсер «завис» в середине кадра (приём оборвался — остались половинки байтов в буфере):**

- добавьте таймаут между байтами. Если после перехода в `Header`/`Payload`/`Crc` прошло больше ~50 мс без новых байт — сбросьте парсер в `WaitSof`. Это защита от неполного кадра, когда второй конец отвалился. В продакшене это обязательно.

**`Hello` ушёл, `HelloAck` не приходит:**

- проверьте, что LINK уже прошёл boot (10–20 секунд на ESP32 с WiFi);
- добавьте retry: повторяйте `Hello` каждые 5 секунд, пока не придёт `HelloAck`;
- проверьте логику сценария B: возможно LINK шлёт `Role::HelloRequest` (`0xFF`), а ваш MCU его не обрабатывает.

---

## Что дальше

На этом этапе вы:

- понимаете, как устроен кадр;
- отправляете и принимаете байты напрямую;
- можете реализовать любые другие типы сообщений по аналогии.

### Следующие шаги

1. **Добавить `Heartbeat`** (раз в 5 секунд) — по структуре `HeartbeatPayload`, см. [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).
2. **Принимать `Command`** (`KIND = 0x20`) — это команды Start/Stop от облака; отвечать `CommandAck`.
3. **Надёжные кадры** — использование флага `ACK_REQUIRED` и retry при таймауте 700 мс; см. [../03-uart/05-ack-retry.md](../03-uart/05-ack-retry.md).
4. **Claiming** (привязка к аккаунту) — отдельный поток сообщений `ClaimStart`/`ClaimStatus`/`ClaimComplete`; см. [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).
5. **Реакция на `Role::HelloRequest`** — добавить колбэк на входящий Hello с role=`0xFF` и отвечать своим полным Hello (см. секцию «Кто говорит первым»).
6. **Retry своего Hello** до первого HelloAck (раз в 5 секунд, пока LINK не ответит).

### Альтернатива: использовать библиотеку целиком

Если ваш MCU — RP2040, STM32 с Arduino-core, ESP32 или другая платформа с поддержкой STL (`std::function`), можно подключить `idryer-protocol` как header-only и взять готовые `struct HelloPayload`, `struct TelemetryPayload`, `calculateCrc()`. Полноценный класс `UartBridge` берёт на себя парсер, ACK/retry и фрагментацию — пример: [03-quickstart-bridge.md](03-quickstart-bridge.md).

---

## Чек-лист перед продакшеном

- [ ] `Hello` отправляется в первые 1–2 секунды после boot.
- [ ] `HelloAck` корректно парсится (проверка длины 37 байт).
- [ ] `Telemetry` отправляется 1 раз/сек в активном режиме, 1 раз/15 сек в idle.
- [ ] `Heartbeat` отправляется каждые 5 сек.
- [ ] Все многобайтовые числа в payload — little-endian.
- [ ] CRC совпал с ожидаемым (сверить на `Hello` из скетча — должно быть `0xE274`).
- [ ] `SEQ` инкрементируется при каждом отправленном кадре.
- [ ] Парсер сбрасывается в `WaitSof` при любой ошибке (CRC, неизвестный KIND, LEN > 200).

---

← [К списку траекторий](01-choose-your-path.md) | [К разделу UART](../03-uart/) →
