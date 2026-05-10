# RFID: Tag Events and Data

Working with RFID readers: reel insertion/removal events and read/write of binary data on the tag.

!!! note "Status"
    **Events** (`tag_detected`/`tag_removed`) work and are used in the app to bind a reel to a specific chamber.
    **Tag data readout** (`commands/read_rfid` → UART `RfidReadData` = 0x1A → MQTT `idryer/<serial>/rfid`) works end-to-end.
    **Writing data to a tag** (`commands/write_rfid` → UART `Command WriteRfid` 0x08 + `RfidWriteData` 0x1B fragments) works end-to-end. Uses stop-and-wait ACK flow control (see below).

---

## Hardware Types

- RFID readers: 0 to 3 units. Identifiers `R1`...`R4` (at indexes 0 to 3).
- One reader is bound to one chamber (`unit`). The mapping is defined in `HelloPayload.UnitConfig.rfid[]`.

---

## Events (`RfidPayload`, 0x14)

### TagDetected

The reel is placed on the reader, or the tag was already present when the device powered on.

**UART payload** (`RfidPayload`):

- `event = TagDetected (1)`
- `readerId = 0..3` (R1..R4)
- `tag` = tag HEX ID, null-terminated, up to 32 characters
- `unitId = 0..3` - which chamber it belongs to

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

The reel is removed from the reader.

**UART:** `event = TagRemoved (2)`, `tag = ""` (empty).

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

!!! note "readerId is a number, unitId is a string"
    In the MQTT publication, `readerId` is a **number** from 0 to 3, while `unitId` is a **string** `"U1"`...`"U4"`. It is asymmetric, as in the other topics. See [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md).

!!! note "No auto-read after TagDetected"
    The UID and the tag contents are published as **two independent events**: `RfidPayload` (0x14) with the UID, and `RfidDataPayload` (0x1A) with the binary contents. The contents are read on an explicit `commands/read_rfid` request. This is deliberate — `tag_detected` is a fast event, while reading contents can take hundreds of milliseconds.

---

## Event Caching

When LINK connects to MQTT, it publishes the **current state of each reader** (retained). This means:

- the last known event - `tag_detected` with the tag ID, or `tag_removed` with an empty tag;
- the guarantee that an app connecting to the `rfid` topic immediately sees the current reel status without waiting.

On the MCU side, the logic is:

- on **boot**, send one event for each reader - `TagDetected` with an ID or `TagRemoved` with an empty tag;
- at runtime - send updates on changes.

---

## Polling rule: only in Idle

Synchronous PN5180 polling blocks the RP2040 main loop for tens of milliseconds. To avoid disrupting PID, LED, and other real-time tasks:

**The MCU polls RFID readers only when ALL chamber controllers are in `DryerMode::Idle`.**

In `Drying`, `Storage`, `Profile`, and `PidAutoTune` modes, background polling is paused. Jobs already in progress (read/write/preview) continue to completion. Explicit `commands/read_rfid` or `commands/write_rfid` requests from the portal are served in any mode — these are targeted operations on request, not background detection.

---

## Reading Tag Data

### UART Command

```
MQTT commands/read_rfid  { "unitId": "U1" }
          ↓
CommandHandler → UART Command{ReadRfid, unitId=0}
```

The MCU requests the tag data blocks from the reader. The amount of data depends on the chip type (see the table below).

### UART Response (RfidReadData, 0x1A)

The response is fragmented at 163 bytes of payload per `RfidDataPayload` frame. **The number of fragments is variable**: `nFrags = ceil(readSize / 163)`, where `readSize` is the chip's actual user-memory volume. LINK detects the end of the chain by `FLAG_LAST_FRAGMENT` and does not check the counter.

| Chip | Read size | Fragments |
|---|---|---|
| NTAG213 | 144 B | 1 |
| SLIX2 (ISO15693) | 316 B | 2 |
| NTAG215 | 504 B | 4 |
| NTAG216 | 888 B | 6 |
| MIFARE Classic 1K | 768 B (48 × 16, block 0 included) | 5 |

!!! note "MIFARE: read vs write"
    MIFARE has different **read** and **write** sizes. Read returns **768 B** (48 blocks × 16, including block 0 with UID/manufacturer data — it is read-only but readable). Write can only update **752 B** (47 user blocks × 16, excluding block 0 and sector trailer blocks). See the [MIFARE Classic 1K](#mifare-classic-1k) section below.

Flags:

- intermediate fragments - `FLAG_FRAGMENTED`,
- the last one - `FLAG_LAST_FRAGMENT`.

**LINK assembles the response by the `FLAG_LAST_FRAGMENT` flag, not by a counter.**

Every frame carries `readerId`, `unitId`, and `tag` to validate that the data really came from this tag.

!!! note "Why 163, not 194 like ConfigPush?"
    `ConfigChunkPayload` (0x30) is pure JSON with a 6-byte chunk header: `200 - 6 = 194` bytes of payload per frame.

    `RfidDataPayload` (0x1A/0x1B) carries **fixed meta fields** (`readerId`, `unitId`, `tag\[32\]`) plus `_pad[2]`, for a total of 36 bytes of overhead. That leaves `199 - 36 = 163` bytes for the tag data itself. That is why the fragment size is different.

Structure - [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

### MQTT Publication

After assembling all fragments, LINK puts the base64-encoded buffer into `idryer/<serial>/rfid` (retained):

```json
{
  "readerId": 0,
  "unitId": "U1",
  "tagId": "04:C1:42:A1:74:26:81",
  "format": "unknown",
  "data": "A/8BL8IcAAAB..."
}
```

The `format` field is a hint to the portal about the content type. Values: `"empty"` (all bytes = 0, `data = null`), `"openprinttag"` (Prusa OpenPrintTag MIME signature found), `"openspool"` (OpenSpool MIME signature found), `"unknown"` (no known signature). Detection happens in the first 128 bytes of the buffer. Full parsing of the content is still done by the portal.

---

## Tag Data Formats

The MCU sends a raw binary; format interpretation is done by the portal.

| Chip | Typical format | Note |
|---|---|---|
| SLIX2 (ISO15693) | OpenPrintTag (Prusa, CBOR + NDEF) | ~316 B |
| NTAG215 | OpenSpool (JSON inside NDEF) | up to 504 B |
| NTAG216 | OpenSpool (JSON) | up to 888 B |
| MIFARE Classic 1K | no open standard | case-specific |

OpenPrintTag starts with an NDEF TLV: `03 FF LL LL C2 1C 00 00 01 0D 61 70 70 6C 69 63 61 74 69 6F 6E 2F 76 6E 64 2E 6F 70 65 6E 70 72 69 6E 74 74 61 67 ...` (MIME `application/vnd.openprinttag`).

---

## Writing Data to a Tag

### MQTT command

```json
MQTT commands/write_rfid
{
  "unitId": "U1",
  "data": "BASE64_STRING...",
  "verify": "header32"
}
```

Parameters:

- `unitId` — chamber identifier `"U1"`...`"U4"`.
- `data` — base64 string, up to 888 bytes after decoding.
- `verify` — post-write readback mode: `"none"` (no check), `"header32"` (first 32 bytes, default), `"full"` (entire buffer). Optional; defaults to `"header32"` when omitted.

### UART chain

LINK decodes base64 and sends to the MCU:

1. `Command WriteRfid` (0x08): `arg0 = rawLen`, `arg1 = verifyCode` (0/1/2).
2. `RfidWriteData` (0x1B) fragments at 163 payload bytes per frame. Intermediate ones — `FLAG_FRAGMENTED`, the last one — `FLAG_LAST_FRAGMENT`.

Fragment count — `ceil(rawLen / 163)`.

### Stop-and-wait ACK flow control

**Every frame in the WriteRfid chain carries `FLAG_ACK_REQUIRED`. LINK does not send the next frame until it has received an ACK for the current one.**

The scheme is based on XMODEM stop-and-wait ARQ and CAN-TP (ISO 15765-2 §9–10) with BlockSize / STmin parameters. This is required because the receiver (RP2040) simultaneously runs PID, polls the scales, and — when allowed — polls the PN5180. The hardware FIFO of the RP2040 UART is 32 bytes (≈2.7 ms at 115200 baud), which overflows if the ISR is delayed. Back-to-back 199-byte fragments with no gap reliably drop bytes → CRC mismatch.

ACK-pacing:

- `LINK → MCU`: `Command WriteRfid` with `FLAG_ACK_REQUIRED`.
- `MCU → LINK`: `CommandAck` with `status = None` (staging armed) or `InvalidPayload` (no reader/tag/size overflow).
- `LINK`: blocks inside `UartBridge::waitForAck(timeoutMs = 200)`, which spins `loop()` to process RX and retry pending frames.
- On success — sends the first fragment with `FLAG_ACK_REQUIRED | FLAG_FRAGMENTED`, waits for ACK. Continues until the last fragment.
- On timeout — aborts the transaction and logs a MQTT `events` entry.

### MCU side

1. When `Command WriteRfid` arrives at the UART handler, it **synchronously** arms the staging buffer via `rfidArmWriteStagingByUnit(unitId, rawLen, verifyCode)`. This is required because LINK sends all frames back-to-back — enqueueing through the main loop would create a window where the first fragment arrives before the staging is armed and gets discarded.
2. At the same time, a sync flag `g_rfidWritePending[reader]` is raised, blocking PN5180 polling on that reader until the transaction completes.
3. `handleUartRfidData` copies each fragment into the staging buffer and **sends an ACK** via `sendCommandAck(sequence, status)`.
4. On the last fragment (`FLAG_LAST_FRAGMENT`), `rfidStartWriteTagData(reader, buffer, len, verifyMode)` is started. An ACK with `status = None` is sent after the write job successfully starts; `InvalidPayload` indicates a size mismatch.

### Limits

- The decoded `data` size must not exceed the chip's writable volume: 144 B for NTAG213, ~316 B for SLIX2, 504 B for NTAG215, 888 B for NTAG216, **752 B for MIFARE Classic 1K** (47 user blocks × 16; block 0 and sector trailer blocks are not written).
- The tag must be on the reader **at the time `write_rfid` is sent and throughout the write operation**. If the tag is removed mid-operation, the PN5180 driver returns an error and clears the staging buffer.
- `ACK timeout = 200 ms` and `MAX_RETRIES = 3` are set in `handleWriteRfid` — exceeding them aborts the transaction.

---

## MIFARE Classic 1K

MIFARE Classic 1K is **read and written** with the default Key A = `FF FF FF FF FF FF`. Limitation: **Bambu tags** (MIFARE tags signed with the manufacturer's RSA key) are read-only — writing requires the private key and is not possible.

There is no open standard format for MIFARE 1K at the time of writing. Applications choose their own data layout.

### Sizes and layout

The reader walks sectors 0..15 in order, authenticating with Key A on the sector trailer. Sector 0 contains the manufacturer block (block 0) with the UID and manufacturer data — this block is **read-only**, a write there is physically not possible. Sector trailer blocks (3, 7, 11, …, 63) hold keys and access bits — the open protocol does not touch them either, to avoid bricking the tag.

| Operation | Volume | Blocks |
|---|---|---|
| Read | 768 B | 0, 1, 2, 4, 5, 6, 8, …, 62 (48 blocks, trailers skipped) |
| Write | 752 B | 1, 2, 4, 5, 6, 8, …, 62 (47 blocks, block 0 and trailers skipped) |

Mapping `userBlockIdx → pageNo` for the MQTT buffer:

- **Read:** `idx ∈ [0..47]` → `pageNo = (idx / 3) * 4 + (idx % 3)` → 0, 1, 2, 4, 5, 6, 8, …
- **Write/verify:** `idx ∈ [0..46]` → first two blocks `idx + 1`, then `4 + (idx−2)/3 * 4 + (idx−2) % 3` → 1, 2, 4, 5, 6, 8, …

The volume asymmetry (768 read vs 752 write) stems from the fact that block 0 is readable but not writable.

### Reliability: sector switch auth retry

When crossing a sector boundary the driver performs `fieldOff → delay → re-SELECT → mifareAuthenticate`. On real hardware the auth for the new sector can fail transiently (the tag NAKs the auth request with `authStatus = 1`) — this is a PN5180 timing quirk between RF field off and re-SELECT.

The MCU PN5180 driver retries the auth up to **3 times**, each attempt running the full cycle `fieldOff → delay (5 ms on the first attempt, 15 ms on retry) → re-SELECT → mifareAuthenticate`. This saves a read/write from a random fail at sector boundaries without data loss.

---

## MCU-Side Details

- `ReadRfid` returns `CommandAck` immediately (the command was accepted); the actual data is then sent in separate `RfidReadData` frames.
- If the tag is absent, the MCU sends `CommandAck` with `status = InvalidPayload` — LINK surfaces the error in `events`.
- If the MCU does not support RFID (`unit.rfid[] = 0xFF` in Hello), LINK skips the UART command entirely and logs an error right away.

---

## What's Next

- [02-websocket-local.md](02-websocket-local.md) - local WebSocket without cloud.
- [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md) - RFID in the general JSON publication list.
- [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md) - `RfidPayload` / `RfidDataPayload`.
- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) - `commands/read_rfid` / `commands/write_rfid`.
