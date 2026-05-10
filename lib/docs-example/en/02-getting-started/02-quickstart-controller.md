# Quickstart: Your Controller (MCU)

The minimal path from an empty IDE to "the first frame went to the LINK module and I got a response". Nothing extra: plain Arduino, one setup, no PlatformIO, no external libraries. After this quickstart you understand the protocol end to end and can move the code to any platform.

!!! note "What Is Here and What Is Not"
    Here you build the UART frame **by hand**: from bytes, with CRC, without a library. That matters so the protocol is not a black box for you.
    After the quickstart, if you want something more compact, you can include `idryer-protocol` as headers only (the structures and `calculateCrc()` are already ready) - see the last section.

!!! warning "Basic Concepts - Read prerequisites"
    The code below uses fixed-width types, bit operations, little-endian, CRC, and `enum class`. If even one of those terms sounds unfamiliar, first go through [../01-overview/05-prerequisites.md](../01-overview/05-prerequisites.md) (10 minutes). Without that base, the quickstart turns into a cipher.

---

## Quickstart Goal

Your controller (MCU) does two things:

1. Sends a `Hello` frame to the LINK module.
2. Sends a `Telemetry` frame once per second with temperature, humidity, and heater power.

The result is a real UART byte stream that LINK accepts and publishes to MQTT.

---

## What You Need

**Hardware:**

- Any Arduino-compatible board **with two hardware UARTs** (ESP32, RP2040 Pico, STM32 Blue Pill, Arduino Mega, etc.). One UART is for USB debugging, the second is for communication with LINK.
- A LINK module (the ready-made iDryer Link board). **Or**, for testing without LINK, a USB-UART adapter and a computer with any binary terminal (Hercules, CoolTerm, `socat`).

!!! warning "Uno / Nano / ATmega328 Are Not Suitable"
    Classic Arduino Uno/Nano boards have only **one** `Serial`, and it is occupied by the USB monitor. UART to LINK requires a second hardware port. `SoftwareSerial` at 115200 baud is unstable - do not try it.

    **Use**: Mega 2560, any ESP32, RP2040 Pico, STM32 Blue Pill, Teensy. They all have `Serial` **and** `Serial1`.

**`Serial1` pins on popular boards:**

| Board | `Serial1` TX | `Serial1` RX | Note |
|-------|--------------|--------------|------|
| Arduino Mega 2560 | 18 | 19 | works out of the box |
| RP2040 Pico (Arduino core) | any | any | call `Serial1.setTX(0); Serial1.setRX(1);` before `Serial1.begin(...)` |
| STM32 Blue Pill | PA9 | PA10 | in Arduino core, `Serial1` is mapped by default |
| ESP32 | any | any | `Serial1.begin(115200, SERIAL_8N1, RX, TX)` |

The sketch below uses `Serial1` by default. Adjust the initialization if your board needs it.

**Software:**

- Arduino IDE or any editor with `avr-gcc` / `arm-gcc`.
- Optional Python 3 for decoding bytes in a terminal.

**Wiring (UART MCU <-> LINK):**

```
   MCU                LINK (ESP32)
  ┌───┐              ┌───┐
  │TX ├──────────────┤RX │
  │RX ├──────────────┤TX │
  │GND├──────────────┤GND│
  └───┘              └───┘
```

Parameters: **115200 baud, 8N1, no hardware flow control**. No resistors or pull-ups are needed - ESP32 uses 3.3 V levels; if your MCU is 5 V (classic Arduino Uno), you need a level shifter.

!!! warning "Power"
    Never connect or disconnect UART cables while power is applied. First turn off power on both boards.

---

## UART Frame Layout (Short Overview)

Before writing code, understand the frame structure. Without that, `0xAA` in the log is meaningless.

```
┌─────┬─────┬──────┬──────┬─────┬─────┬─────────────┬───────────┐
│ SOF │ VER │FLAGS │ KIND │ SEQ │ LEN │   PAYLOAD   │   CRC16   │
│ 1B  │ 1B  │  1B  │  1B  │ 1B  │ 1B  │ 0-200 bytes │  2B (LE)  │
└─────┴─────┴──────┴──────┴─────┴─────┴─────────────┴───────────┘
```

| Field | Size | Meaning |
|------|--------|----------|
| SOF | 1 byte | Always `0xAA` - frame start marker |
| VER | 1 byte | Protocol version, always `0x01` |
| FLAGS | 1 byte | Bit flags (ACK, error, fragmentation) |
| KIND | 1 byte | Message type (`0x01` = Hello, `0x10` = Telemetry, etc.) |
| SEQ | 1 byte | Counter 0-255, incremented on each frame |
| LEN | 1 byte | Payload length, 0-200 |
| PAYLOAD | LEN bytes | Data, structure depends on KIND |
| CRC | 2 bytes | CRC16-CCITT over `[SOF ... end of PAYLOAD]`, stored little-endian |

**All multi-byte values in the payload are little-endian.** That matters. `uint32_t = 0x00010000` (firmware version 1.0.0) is written as `00 00 01 00`.

**CRC is calculated over everything before it** - from SOF inclusive to the last byte of the payload. Not including the CRC itself.

The full field and structure reference is here: [../03-uart/](../03-uart/). Here you only get what you need right now.

---

## Who Speaks First

The handshake between MCU and LINK is symmetrical - either side can initiate.

**Scenario A - MCU starts first:**

1. MCU sends `Hello` with role=`0x01` (MCU).
2. If LINK is already ready, it responds with `HelloAck` containing IP/SSID.
3. If not, the frame may be lost in transit; the MCU must keep retrying `Hello` until it gets a response.

**Scenario B - LINK starts first:**

1. LINK sends an empty `Hello` with role=`0xFF` (`HelloRequest`) - this is a trigger: "hey MCU, send me your Hello".
2. MCU sees `Hello` with role=`0xFF` and **in response** sends its full `Hello`.
3. LINK replies with `HelloAck`.

**What your MCU must be able to do:**

- send its `Hello` once on boot;
- listen for incoming frames at the same time; if a `Hello` with role=`0xFF` arrives, send the full `Hello` again;
- optionally, repeat Hello every 5 seconds until the first `HelloAck` arrives (LINK may still be booting or reconnecting WiFi and may not answer immediately).

The mini sketch in the next step does only the first part: one `Hello` in `setup()`. For production, add retries and reaction to `Role::HelloRequest` (`0xFF`).

---

## Step 1. CRC16-CCITT Implementation

Write it once and forget it. Polynomial `0x1021`, initial value `0xFFFF`.

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

It matches the reference implementation in the library (`src/uart/uart_protocol.cpp`).

---

## Step 2. Minimal Sketch

A complete working example for Arduino. Sends Hello at startup and Telemetry once per second.

```cpp
#include <Arduino.h>
#include <string.h>

// ============================================================================
// iDryer UART protocol constants
// ============================================================================
constexpr uint8_t SOF = 0xAA;
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t MAX_PAYLOAD = 200;

// Message types (MessageKind)
constexpr uint8_t KIND_HELLO     = 0x01;
constexpr uint8_t KIND_HELLO_ACK = 0x02;
constexpr uint8_t KIND_TELEMETRY = 0x10;

// Role
constexpr uint8_t ROLE_MCU = 0x01;

// DeviceType
constexpr uint8_t DEVICE_DRYER = 0x01;

// Capability bits
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
// Frame sending
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
// Build Hello payload (86 bytes)
// HelloPayload structure - see docs/03-uart/04-binary-structures.md
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

    // units[0]: unitId=0, caps=HEATER|FAN|RH|TEMP_AIR, scales and rfid = 0xFF (unused)
    uint16_t caps = CAP_HEATER | CAP_FAN | CAP_RH_AIR | CAP_TEMP_AIR;
    payload[i++] = 0;                // unitId
    payload[i++] = 0;                // _pad1
    memcpy(&payload[i], &caps, 2); i += 2;
    memset(&payload[i], 0xFF, 4);  i += 4;   // scales
    memset(&payload[i], 0xFF, 4);  i += 4;   // rfid
    // units[1..3] remain zeroed (unitsCount=1)
    constexpr size_t UNIT_CONFIG_SIZE = 12;
    i += UNIT_CONFIG_SIZE * 3;

    // mcuSerial[17] - 16 hex digits + '\0'
    strncpy((char*)&payload[i], "36B955AB4350FEDC", 17);
    // i += 17;   // total i should be 86

    sendFrame(KIND_HELLO, payload, sizeof(payload));
}

// ============================================================================
// Build Telemetry payload
// Variant: one chamber, the second half of the array stays zeroed
// ============================================================================
void sendTelemetry(float tempC, float humidityPct, uint8_t heaterPct, bool fanOn) {
    uint8_t payload[29] = {0};  // count(1) + 4*TelemetryEntry(7) = 29
    payload[0] = 1;             // count = one chamber filled

    int16_t  t10 = (int16_t)(tempC * 10.0f);
    uint16_t h10 = (uint16_t)(humidityPct * 10.0f);

    // TelemetryEntry[0] - 7 bytes
    payload[1] = 0;                              // unitId = 0
    memcpy(&payload[2], &t10, 2);                // temperatureC10 (LE)
    memcpy(&payload[4], &h10, 2);                // humidityPct10 (LE)
    payload[6] = heaterPct;                      // heaterPowerPct
    payload[7] = fanOn ? 1 : 0;                  // fanOn

    // Entries 1..3 are zeroed. LINK sees count=1 and reads only the first one.

    sendFrame(KIND_TELEMETRY, payload, sizeof(payload));
}

// ============================================================================
// Setup & loop
// ============================================================================
uint32_t lastTelemetry = 0;

void setup() {
    Serial.begin(115200);           // debug port
    Serial1.begin(115200);          // to LINK (depends on the board: pin mapping may differ)

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

    // Incoming frames - simplified: read and print hex (full parser is in Step 4)
    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        Serial.print(b, HEX); Serial.print(' ');
    }
}
```

Upload it and open Serial Monitor at 115200. You will see `MCU start, sending Hello...`, then once per second `Telemetry sent` and, if a real LINK is connected, incoming bytes from it.

---

## Step 3. What the Frame Looks Like on the Wire

### HelloPayload Layout (86 bytes)

Before looking at the hex dump, it helps to see how the payload fields are laid out. Total: exactly 86 bytes:

| Offset | Size | Field | Description |
|--------|------|------|-------------|
| 0 | 1 | `role` | 0x01 = MCU |
| 1 | 1 | `deviceType` | 0x01 = Dryer |
| 2 | 2 | `_pad1[2]` | alignment |
| 4 | 4 | `firmwareVersion` | uint32 LE: `(MAJOR<<16)\|(MINOR<<8)\|PATCH` |
| 8 | 4 | `workTimeCounter` | uint32 LE: uptime in seconds |
| 12 | 8 | `hardwareVersion[8]` | ASCII `"v1.0\0\0\0\0"` |
| 20 | 1 | `unitsCount` | 0-4 |
| 21 | 48 | `units[4]` | 4 x `UnitConfig` at 12 bytes each |
| 69 | 17 | `mcuSerial[17]` | 16 hex + `\0` |

**Total:** 1+1+2+4+4+8+1+48+17 = **86**.

**`UnitConfig`** (12 bytes):

| Offset | Size | Field |
|--------|------|------|
| 0 | 1 | `unitId` (0-3) |
| 1 | 1 | `_pad1` |
| 2 | 2 | `capabilities` (uint16 LE, bit flags) |
| 4 | 4 | `scales[4]` (scale sensor indices, `0xFF` = none) |
| 8 | 4 | `rfid[4]` (RFID reader indices, `0xFF` = none) |

Unused `units[1..3]` in the sketch are 36 zero bytes. They are required for the fixed 86-byte length; LINK does not read them because `unitsCount = 1`.

### Full Hello Hex Dump

You send `Hello` with the parameters from the sketch. Here is the **complete byte dump** that goes to UART:

```
Offset       Hex bytes                                              Description
──────────── ─────────────────────────────────────────────────────  ─────────────────────
0x00         AA                                                     SOF
0x01         01                                                     VER = 1
0x02         00                                                     FLAGS = 0
0x03         01                                                     KIND = Hello
0x04         00                                                     SEQ = 0
0x05         56                                                     LEN = 86

--- payload (86 bytes) ---
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
0x27..0x4A   (36 zero bytes)                                        unit[1..3] - empty (unitsCount=1)
0x4B..0x5B   33 36 42 39 35 35 41 42 34 33 35 30 46 45 44 43 00     mcuSerial "36B955AB4350FEDC\0"

--- CRC (2 bytes, little-endian) ---
0x5C..0x5D   74 E2                                                  CRC16 = 0xE274 -> low byte first
```

That is 94 bytes total. In a terminal, the continuous stream looks like this:

```
AA 01 00 01 00 56 01 01 00 00 00 00 01 00 00 00 00 00 76 31
2E 30 00 00 00 00 01 00 00 1B 00 FF FF FF FF FF FF FF FF 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 33 36 42 39
35 35 41 42 34 33 35 30 46 45 44 43 00 74 E2
```

### Telemetry Looks Like This Too (37 bytes):

```
AA 01 00 10 00 1D 01 00 29 02 C4 01 50 01 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 DA
```

Breakdown:

```
Offset       Hex bytes                              Description
──────────── ──────────────────────────────────────  ──────────────────────────
0x00         AA                                     SOF
0x01         01                                     VER = 1
0x02         00                                     FLAGS = 0
0x03         10                                     KIND = Telemetry
0x04         00                                     SEQ = 0
0x05         1D                                     LEN = 29

--- payload (29 bytes) ---
0x06         01                                     count = 1
0x07         00                                     entry[0].unitId = 0
0x08..0x09   29 02                                  temperatureC10 = 0x0229 = 553 -> 55.3 °C
0x0A..0x0B   C4 01                                  humidityPct10  = 0x01C4 = 452 -> 45.2 %
0x0C         50                                     heaterPowerPct = 80
0x0D         01                                     fanOn = 1
0x0E..0x22   (21 zero bytes)                        entry[1..3] unused (count=1)

--- CRC (little-endian) ---
0x23..0x24   10 DA                                  CRC16 = 0xDA10
```

You can **manually verify the CRC** with any online calculator. Choose **CRC-16/CCITT-FALSE** (also **CRC-16/IBM-3740**): polynomial `0x1021`, init `0xFFFF`, RefIn/RefOut = false, XorOut = `0x0000`.

!!! warning "Do Not Confuse It with XMODEM"
    Online calculators often list **CCITT-FALSE (init 0xFFFF)** and **XMODEM (init 0x0000)** side by side. iDryer uses the first one. If you pick XMODEM, you will get a different value and start suspecting your code. Check the init value first.

Feed the first 35 bytes of the Telemetry frame (without the last two). You should get `0xDA10`. For Hello, feed the first 92 bytes - you get `0xE274`.

**Verification without hardware (Python):**

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

This function is a 1:1 copy of the C implementation in the library (`src/uart/uart_protocol.cpp`).

---

## Step 4. Receiving HelloAck

LINK will reply with a `HelloAck` frame (KIND=`0x02`, LEN=37). To parse it, you need a tiny parser. Add this to the sketch:

```cpp
// Header field indices (so we do not write magic hdr[3], hdr[5])
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

// Declaration of handleFrame - implementation below
void handleFrame(uint8_t kind, const uint8_t *payload, uint8_t len);

// Transition to Crc state with rxCrc reset - shared by both paths
// (LEN=0 straight from Header, LEN>0 from Payload)
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
            // Basic validation: version and payload length
            if (hdr[HDR_VER] != PROTOCOL_VERSION || hdr[HDR_LEN] > MAX_PAYLOAD) {
                parser_state = ParserState::WaitSof;
                return;
            }
            plIdx = 0;
            if (hdr[HDR_LEN] == 0) {
                gotoCrcState();          // frame without payload (for example, HelloRequest)
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

            // Merge header + payload and compute expected CRC
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

Replace the incoming-frames block in `loop()` with:

```cpp
while (Serial1.available()) {
    parseByte(Serial1.read());
}
```

If everything works, Serial Monitor shows:

```
[HelloAck] IP=192.168.1.5 SSID=MyHomeNetwork
```

---

## If Something Does Not Work

**The frame does not reach LINK (LINK stays silent):**

- check the crossover: `MCU TX -> LINK RX`, `MCU RX <- LINK TX`, shared `GND`;
- check voltage levels - if the MCU is 5 V, you need a level shifter to 3.3 V ESP32;
- probe the line with an oscilloscope or logic analyzer at 115200 baud - you should see byte `0xAA` at the start.

**Garbage arrives in the MCU (random bytes):**

- the baud rates differ on the two sides. Make sure LINK is also at 115200.
- poor GND contact - add a common ground.

**CRC does not match:**

- verify that you calculate CRC over **all** header bytes + payload, **excluding** the CRC itself.
- verify init: `0xFFFF`, not `0x0000` (a common mistake).
- feed the reference Telemetry array (35 bytes) and compare it with `0xDA10`.

**The parser loops on a random `0xAA` byte inside the payload:**

- this can happen and is normal. The parser starts assembling a frame from the false SOF, but after the CRC check it drops it and returns to `WaitSof`. No additional action is needed.

**The parser got stuck in the middle of a frame (the receive stream broke and half-bytes are left in the buffer):**

- add a timeout between bytes. If more than about 50 ms passes after entering `Header`/`Payload`/`Crc` without new bytes, reset the parser to `WaitSof`. This protects against a partial frame when the other end disappears. In production this is mandatory.

**`Hello` was sent, but `HelloAck` never arrives:**

- check that LINK has already finished booting (10-20 seconds on ESP32 with WiFi);
- add retries: resend `Hello` every 5 seconds until `HelloAck` arrives;
- check scenario B logic: LINK may be sending `Role::HelloRequest` (`0xFF`), and your MCU may not handle it.

---

## What Next

At this point you:

- understand how the frame is built;
- send and receive bytes directly;
- can implement any other message types by analogy.

### Next Steps

1. **Add `Heartbeat`** (every 5 seconds) using the `HeartbeatPayload` structure, see [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).
2. **Receive `Command`** (`KIND = 0x20`) - these are Start/Stop commands from the cloud; reply with `CommandAck`.
3. **Reliable frames** - use the `ACK_REQUIRED` flag and retry on a 700 ms timeout; see [../03-uart/05-ack-retry.md](../03-uart/05-ack-retry.md).
4. **Claiming** (binding to an account) - a separate message flow of `ClaimStart`/`ClaimStatus`/`ClaimComplete`; see [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).
5. **React to `Role::HelloRequest`** - add a callback for incoming Hello with role=`0xFF` and answer with your full Hello (see "Who Speaks First").
6. **Retry your Hello** until the first HelloAck arrives (every 5 seconds, until LINK answers).

### Alternative: Use the Library as a Whole

If your MCU is RP2040, STM32 with Arduino core, ESP32, or another platform with STL (`std::function`) support, you can include `idryer-protocol` as header-only and use the ready-made `struct HelloPayload`, `struct TelemetryPayload`, and `calculateCrc()`. The full `UartBridge` class handles parsing, ACK/retry, and fragmentation for you - see [03-quickstart-bridge.md](03-quickstart-bridge.md).

---

## Pre-Production Checklist

- [ ] `Hello` is sent within the first 1-2 seconds after boot.
- [ ] `HelloAck` is parsed correctly (check that its length is 37 bytes).
- [ ] `Telemetry` is sent once per second in active mode, once every 15 seconds in idle mode.
- [ ] `Heartbeat` is sent every 5 seconds.
- [ ] All multi-byte values in the payload are little-endian.
- [ ] The CRC matches the expected value (verify on the `Hello` from the sketch - it should be `0xE274`).
- [ ] `SEQ` increments for every sent frame.
- [ ] The parser resets to `WaitSof` on any error (CRC, unknown KIND, LEN > 200).

---

← [Back to paths](01-choose-your-path.md) | [UART protocol in detail](../03-uart/) →
