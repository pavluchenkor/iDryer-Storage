# Frame Examples (Live Hex Dumps)

Concrete bytes that you should see on UART. Useful for checking your parser or your frame builder. All CRCs are real and were calculated with the reference `DryerUart::calculateCrc()` function.

!!! note
    Structures and offsets - [04-binary-structures.md](04-binary-structures.md). Types and purpose - [03-message-types.md](03-message-types.md).

---

## Hello (MCU -> LINK)

**Example parameters:** `role=MCU`, `deviceType=Dryer`, firmware `1.0.0`, hardwareVersion `"v1.0"`, `unitsCount=1`, unit[0] capabilities `HEATER|FAN|RH_AIR|TEMP_AIR` (`0x001B`), mcuSerial `"36B955AB4350FEDC"`.

**Full dump (94 bytes):**

```
AA 01 00 01 00 56 01 01 00 00 00 00 01 00 00 00 00 00 76 31
2E 30 00 00 00 00 01 00 00 1B 00 FF FF FF FF FF FF FF FF 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 33 36 42 39
35 35 41 42 34 33 35 30 46 45 44 43 00 74 E2
```

**CRC = 0xE274.** Detailed offset breakdown is in [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md) (section "Step 3").

---

## HelloAck (LINK -> MCU)

**Parameters:** IP `192.168.1.5`, SSID `"MyNet"`.

```
Offset       Hex bytes                                          Description
──────────── ──────────────────────────────────────────────────  ──────────────────────
0x00         AA                                                 SOF
0x01         01                                                 VER
0x02         00                                                 FLAGS
0x03         02                                                 KIND = HelloAck
0x04         00                                                 SEQ = 0
0x05         25                                                 LEN = 37

0x06..0x09   05 01 A8 C0                                        ipAddress = 192.168.1.5 (LE)
0x0A..0x2A   4D 79 4E 65 74 00 ...                              ssid = "MyNet\0\0..." (33 bytes total)

0x2B..0x2C   XX XX                                              CRC16 (depends on SSID contents)
```

---

## Telemetry (MCU -> LINK)

**Parameters:** 1 unit, 55.3 C, 45.2 %, heater 80 %, fan on.

```
Full frame (37 bytes):
AA 01 00 10 00 1D 01 00 29 02 C4 01 50 01 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 DA
```

**CRC = 0xDA10.** See the controller quickstart for details.

---

## Heartbeat (MCU -> LINK)

**Parameters:** uptime 12 s, `wifiRssiDbm` field = 450 (the firmware writes MCU temperature x 10 there = 45.0 C), errors 0, `cloudState` = 0 (the field is not used in this direction).

```
Offset       Hex bytes               Description
──────────── ───────────────────────  ───────────────────────────
0x00         AA                      SOF
0x01         01                      VER
0x02         00                      FLAGS = 0
0x03         40                      KIND = Heartbeat
0x04         05                      SEQ = 5
0x05         09                      LEN = 9

0x06..0x09   0C 00 00 00             uptimeSeconds = 12
0x0A..0x0B   C2 01                   wifiRssiDbm = 0x01C2 = 450
0x0C..0x0D   00 00                   errorsSinceBoot = 0
0x0E         00                      cloudState = 0

0x0F..0x10   E7 53                   CRC16 = 0x53E7 (LE)
```

Full frame: `AA 01 00 40 05 09 0C 00 00 00 C2 01 00 00 00 E7 53`.

---

## Command: Start Drying (LINK -> MCU, requires ACK)

**Parameters:** Start in Drying mode, unit 0 (U1), target = 55.0 C, duration = 120 min.

```
Offset       Hex bytes                                  Description
──────────── ───────────────────────────────────────────  ─────────────────────────────
0x00         AA                                         SOF
0x01         01                                         VER
0x02         01                                         FLAGS = FLAG_ACK_REQUIRED
0x03         20                                         KIND = Command
0x04         0A                                         SEQ = 10
0x05         0D                                         LEN = 13

0x06         01                                         command = Start
0x07         01                                         targetState = DryerMode::Drying
0x08         00                                         unitId = 0
0x09..0x0A   00 00                                      reserved[2]
0x0B..0x0E   26 02 00 00                                arg0 = 550 (temperature x 10)
0x0F..0x12   78 00 00 00                                arg1 = 120 (minutes)

0x13..0x14   27 C6                                      CRC16 = 0xC627 (LE)
```

Full frame: `AA 01 01 20 0A 0D 01 01 00 00 00 26 02 00 00 78 00 00 00 27 C6`.

---

## CommandAck (MCU -> LINK)

**Parameters:** acknowledgment for `Command` with SEQ=10, status OK.

```
Offset       Hex bytes        Description
──────────── ────────────────  ──────────────────────
0x00         AA               SOF
0x01         01               VER
0x02         02               FLAGS = FLAG_IS_ACK
0x03         21               KIND = CommandAck
0x04         0A               SEQ = 10 (original Command number)
0x05         02               LEN = 2

0x06         0A               ackSequence = 10
0x07         00               status = None (OK)

0x08..0x09   6F 5E            CRC16 = 0x5E6F (LE)
```

Full frame: `AA 01 02 21 0A 02 0A 00 6F 5E`.

---

## ClaimStart (MCU -> LINK, empty payload)

**Parameters:** claim start request. No payload is present.

```
Offset       Hex bytes        Description
──────────── ────────────────  ──────────────────────
0x00         AA               SOF
0x01         01               VER
0x02         01               FLAGS = FLAG_ACK_REQUIRED
0x03         70               KIND = ClaimStart
0x04         03               SEQ = 3
0x05         00               LEN = 0

0x06..0x07   84 38            CRC16 = 0x3884 (LE)
```

Full frame (8 bytes): `AA 01 01 70 03 00 84 38`.

This is the shortest possible frame in the protocol (zero payload + header + CRC).

---

## ClaimStatus: WaitingClaim (LINK -> MCU)

**Parameters:** waiting for user PIN entry. PIN = `"12345678"`, expires at `expiresAt = 1768320000` (unix timestamp), 600 seconds remaining.

```
Offset       Hex bytes                                          Description
──────────── ──────────────────────────────────────────────────  ───────────────────────────
0x00         AA                                                 SOF
0x01         01                                                 VER
0x02         00                                                 FLAGS
0x03         71                                                 KIND = ClaimStatus
0x04         04                                                 SEQ = 4
0x05         12                                                 LEN = 18

0x06         02                                                 status = WaitingClaim
0x07..0x0F   31 32 33 34 35 36 37 38 00                         pin = "12345678\0"
0x10..0x13   00 6C 66 69                                        expiresAt = 0x6966_6C00 = 1768320000 (LE)
0x14..0x17   58 02 00 00                                        remainingSeconds = 600 (LE)

0x18..0x19   0F 9C                                              CRC16 = 0x9C0F (LE)
```

Full frame: `AA 01 00 71 04 12 02 31 32 33 34 35 36 37 38 00 00 6C 66 69 58 02 00 00 0F 9C`.

---

## Status (MCU -> LINK)

**Parameters:** 1 unit, Drying mode, sessionNum = 42, target = 55.0 C, duration = 120 min, elapsed = 360 s, totalRemaining = 6840 s. Device uptime = 3600 s.

```
Full frame (141 bytes):
AA 01 00 13 0E 85 01 00 01 2A 00 00 00 26 02 00
00 78 00 68 01 00 00 00 00 00 00 00 00 00 00 B8
1A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 10 0E 00 00 1C 6D
```

**CRC = 0x6D1C.** Breakdown:

- `0x00..0x05` - header: `AA 01 00 13 0E 85` (KIND=0x13, SEQ=14, LEN=133).
- `0x06` - `count = 1`.
- `0x07..0x26` - `StatusEntry[0]` (32 bytes): `unitId=0, mode=Drying(1), session=42 (0x2A), targetTemp=550 (0x0226), durationMin=120 (0x0078), elapsed=360 (0x00000168), stageElapsed=0, stageRem=0, totalRem=6840 (0x00001AB8), stage fields = 0`.
- `0x27..0x86` - `StatusEntry[1..3]` are zeroed (count=1).
- `0x87..0x8A` - `uptime = 3600 (0x00000E10)`.
- `0x8B..0x8C` - `CRC = 0x6D1C` (LE).

---

## Command with ProfilePayload (LINK -> MCU, 64 bytes)

The same `MessageKind = 0x20`, but **payload length = 64** instead of 13 - this is the signal that the payload is `ProfilePayload`, not `CommandPayload`.

**Parameters:** unit 0, 2 stages:

- Stage 1: 60.0 C, ramp = 300 s, hold = 1800 s.
- Stage 2: 100.0 C, ramp = 600 s, hold = 6000 s.

```
Full frame (72 bytes):
AA 01 01 20 0B 40 00 02 00 00 58 02 2C 01 08 07
E8 03 58 02 70 17 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 95 0D
```

**CRC = 0x0D95.** Breakdown:

- `0x00..0x05` - header: `AA 01 01 20 0B 40` (FLAGS=ACK_REQUIRED, SEQ=11, LEN=**64** -> ProfilePayload).
- `0x06..0x09` - `unitId=0, totalStages=2, startStage=0, _pad=0`.
- `0x0A..0x0F` - stage[0]: `temp=600 (0x0258), ramp=300 (0x012C), hold=1800 (0x0708)`.
- `0x10..0x15` - stage[1]: `temp=1000 (0x03E8), ramp=600 (0x0258), hold=6000 (0x1770)`.
- `0x16..0x45` - stage[2..9] are zeroed.
- `0x46..0x47` - CRC.

If the receiver sees `kind == 0x20, len == 13`, it is an ordinary `CommandPayload`. If `len == 64`, it is a profile.

---

## ConfigPush: Fragmented Config (First Chunk)

Transfer of a 300-byte JSON config (example). This is the **first of two** fragments, carrying bytes 0..193 and a header with `totalSize`.

```
Full frame (208 bytes):
AA 01 09 30 15 C8 34 12 2C 01 00 00 7B 22 76 22
3A 38 2C 22 75 6E 69 74 73 22 3A 33 2C 22 61 63
74 69 76 65 22 3A 30 2C 22 6C 61 6E 67 22 3A 22
65 6E 22 2C 22 6D 65 6E 75 22 3A 5B 7B 22 69 64
22 3A 33 2C 22 74 22 3A 22 76 61 6C 22 2C 22 76
61 6C 22 3A 5B 35 30 2C 36 35 2C 38 35 5D 7D 2C
7B 22 69 64 22 3A 35 2C 22 74 22 3A 22 76 61 6C
22 2C 22 76 61 6C 22 3A 5B 34 30 2C 31 35 5D 7D
2C 7B 22 69 64 22 3A 31 37 2C 22 74 22 3A 22 76
61 6C 22 2C 22 76 61 6C 22 3A 5B 35 35 2C 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 CE 97
```

**CRC = 0x97CE.** Breakdown:

- `0x00..0x05` - header: `AA 01 09 30 15 C8`.
  - `FLAGS = 0x09 = FLAG_ACK_REQUIRED | FLAG_FRAGMENTED` (ACK is expected, and this is not the last fragment).
  - `KIND = 0x30` (ConfigPush), `SEQ = 21`, `LEN = 200` (maximum).
- `0x06..0x07` - `transferId = 0x1234` (LE).
- `0x08..0x09` - `totalSize = 300` (LE; filled in the first fragment).
- `0x0A..0x0B` - `chunkIndex = 0` (LE).
- `0x0C..0xCD` - `data[194]`: JSON bytes 0..193.
- `0xCE..0xCF` - CRC.

The second fragment will have `FLAGS = FLAG_ACK_REQUIRED | FLAG_LAST_FRAGMENT`, `chunkIndex = 1`, `totalSize = 0` (not filled in later chunks), and `LEN` = the remaining JSON bytes + 6 bytes of chunk header.

---

## Log (MCU -> LINK, Structured Event)

**Parameters:** severity `"error"`, source `"HEATER"`, event `"OVER_MAX"`, message `"Heater over max temperature"`, unit 0.

```
Full frame (172 bytes):
AA 01 00 60 20 A4 65 72 72 6F 72 00 00 00 00 00
48 45 41 54 45 52 00 00 00 00 00 00 00 00 00 00
00 00 00 00 4F 56 45 52 5F 4D 41 58 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 48 65 61 74 65 72 20 6F 76 65 72 20
6D 61 78 20 74 65 6D 70 65 72 61 74 75 72 65 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 4D C9
```

**CRC = 0xC94D.** Breakdown:

- `0x00..0x05` - header: `AA 01 00 60 20 A4` (KIND=0x60, SEQ=32, LEN=164).
- `0x06..0x0F` - `severity[10]`: `"error\0\0\0\0\0"` - ASCII + zero padding.
- `0x10..0x23` - `source[20]`: `"HEATER\0..."`.
- `0x24..0x43` - `event[32]`: `"OVER_MAX\0..."`.
- `0x44..0xA7` - `message[100]`: `"Heater over max temperature\0..."`.
- `0xA8` - `unitId = 0`.
- `0xA9` - `_pad = 0`.
- `0xAA..0xAB` - CRC.

---

## Weights (MCU -> LINK)

**Parameters:** 1 sensor (W1), bound to unit 0, weight 823.4 g (= 8234).

```
Offset       Hex bytes                              Description
──────────── ──────────────────────────────────────  ──────────────────────────
0x00         AA                                     SOF
0x01         01                                     VER
0x02         00                                     FLAGS
0x03         12                                     KIND = Weights
0x04         07                                     SEQ = 7
0x05         11                                     LEN = 17

0x06         01                                     count = 1
0x07         00                                     WeightEntry[0].sensorId = 0 (W1)
0x08         00                                     WeightEntry[0].unitId = 0 (U1)
0x09..0x0A   2A 20                                  weightGramsC10 = 8234 -> 823.4 g
0x0B..0x16   (12 bytes of zeros)                    WeightEntry[1..3] are unused

0x17..0x18   D6 0F                                  CRC16 = 0x0FD6 (LE)
```

Full frame: `AA 01 00 12 07 11 01 00 00 2A 20 00 00 00 00 00 00 00 00 00 00 00 00 D6 0F`.

---

## Rfid: TagDetected (MCU -> LINK)

**Parameters:** reader 0 (R1), tag `"DEADBEEF12345678"`, unit 0.

```
Offset       Hex bytes                                          Description
──────────── ──────────────────────────────────────────────────  ──────────────────────────
0x00..0x05   AA 01 00 14 09 25                                  header (KIND=Rfid, SEQ=9, LEN=37)
0x06         01                                                 event = TagDetected
0x07         00                                                 readerId = 0 (R1)
0x08..0x27   44 45 41 44 42 45 45 46 31 32 33 34 35 36 37 38    tag = "DEADBEEF12345678\0..."
             00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x28         00                                                 unitId = 0 (U1)
0x29..0x2A   00 00                                              _pad[2]

0x2B..0x2C   6B 1A                                              CRC16 = 0x1A6B (LE)
```

Full frame (45 bytes): `AA 01 00 14 09 25 01 00 44 45 41 44 42 45 45 46 31 32 33 34 35 36 37 38 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 6B 1A`.

---

## Error: CrcMismatch

**Parameters:** the sender received a frame with a broken CRC. The broken frame had SEQ = 42.

```
Offset       Hex bytes              Description
──────────── ──────────────────────  ───────────────────────────
0x00         AA                     SOF
0x01         01                     VER
0x02         04                     FLAGS = FLAG_ERROR
0x03         50                     KIND = Error
0x04         00                     SEQ = 0 (our new frame)
0x05         04                     LEN = 4

0x06         01                     code = CrcMismatch
0x07         2A                     lastSequence = 42 (culprit)
0x08..0x09   00 00                  detail = 0

0x0A..0x0B   AD 18                  CRC16 = 0x18AD (LE)
```

Full frame: `AA 01 04 50 00 04 01 2A 00 00 AD 18`.

Note that `FLAG_ERROR` is set in the header not because "this frame arrived with an error", but because "this frame carries `ErrorPayload`". This marks diagnostic traffic explicitly.

---

## Checking Your Own Parser

Take any full dump above. Feed it into your code:

1. Run all bytes up to the CRC through your `crc16()` function (formula in [02-frame-and-crc.md](02-frame-and-crc.md)).
2. Compare it with the listed CRC. It must match bit for bit.
3. Parser - verify every offset from the table.
4. Check `payloadLength` against the expected length for that `kind` (see [03-message-types.md](03-message-types.md)).

If one of the reference CRCs does not match in your implementation, the problem is almost certainly one of these:

- byte order is reversed (you are writing big-endian instead of little-endian);
- CRC initial value is not `0xFFFF` (maybe `0x0000` or another CCITT variant);
- the header was not included in the CRC calculation, only the payload.

---

## What Next

- [05-ack-retry.md](05-ack-retry.md) - ACK frame rules and retry.
- [../04-mqtt/](../04-mqtt/) - how these UART data become MQTT messages.
- [../06-flows/](../06-flows/) - end-to-end scenarios where frames are sent in sequence.
