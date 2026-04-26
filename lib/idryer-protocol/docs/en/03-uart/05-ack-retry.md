# ACK, Retry, Fragmentation

Reliable delivery in the iDryer UART protocol is built on three mechanisms:

1. **ACK** - acknowledgment by a separate frame.
2. **Retry** - retransmission on timeout.
3. **Fragmentation** - splitting large payloads into chunks.

All three are handled by `DryerUart::UartBridge`. If you are writing your own parser manually, this page describes all the rules you need to follow.

---

## ACK - Frame Acknowledgment

### When It Is Needed

ACK is required if the sender sets the `FLAG_ACK_REQUIRED = 0x01` flag in the header. Without that flag, the frame is fire-and-forget: if it arrives, good; if not, nobody will know.

### Which Message Types Require ACK by Default

The decision of whether to set the flag is made by the **sender**. In the reference firmware:

| Type | ACK by default |
|------|----------------|
| `Telemetry` (`0x10`) | optional (often without it - the next data will arrive anyway) |
| `Weights` (`0x12`) | optional |
| `Status` (`0x13`) | optional |
| `Rfid` (`0x14`) | optional |
| `Command` (`0x20`) | **yes by default** (`sendCommand` in the API sets `ackRequired = true`) |
| `ConfigPush` (`0x30`) | **yes by default** |
| `ClaimStart` (`0x70`) | **yes by default** |
| `Hello` / `HelloAck` | no (handshake without ACK) |
| `Heartbeat` | no |
| `Error` | no |
| `Log` | no |

### ACK Frame Format

A separate frame with the matching ACK type:

| Source Type | ACK Type | Code |
|-------------|----------|------|
| `Telemetry` | `TelemetryAck` | `0x11` |
| `Command` | `CommandAck` | `0x21` |
| `ConfigPush` | `ConfigAck` | `0x31` |

All three use the same `AckPayload` structure (2 bytes): `ackSequence` + `status` (ErrorCode). In the ACK frame **header**, the `sequence` field is also equal to the number of the frame being acknowledged, not the ACK's own number - this makes matching easier.

For the other types with `FLAG_ACK_REQUIRED`, there is no separate ACK type; the receiver responds with an `Error` frame on failure or with nothing on success (which is equivalent to a `Timeout` on the sender side).

### ACK Example

Acknowledging `Command` with SEQ=7, status OK:

```
[Header]
  AA       SOF
  01       VER
  02       FLAGS = FLAG_IS_ACK
  21       KIND = CommandAck
  07       SEQ = 7 (original Command number)
  02       LEN = 2

[Payload - AckPayload]
  07       ackSequence = 7
  00       status = None (OK)

[CRC]
  XX XX    CRC16 LE
```

---

## Retry - Retransmission

### Parameters

Defined in `uart_protocol.h`:

| Constant | Value | Purpose |
|----------|-------|---------|
| `COMMAND_REPLY_TIMEOUT_MS` | 700 ms | ACK wait timeout |
| `MAX_RETRIES` | 3 | Maximum number of retries |

### Algorithm

1. Send a frame with `FLAG_ACK_REQUIRED`.
2. Store the frame as pending and start the timer.
3. In parallel, wait for any incoming frame.
4. After 700 ms from sending, if no ACK arrived and `retries < 3`, resend **the same frame with the same SEQ** and increment the counter.
5. After the third timeout, report `Error{Timeout}` to `errorHandler_` and clear the pending entry.

### If the Correct ACK Arrives

- Match by `ackSequence` in the payload or by `sequence` in the header (they must match).
- If a pending frame with that SEQ is found, clear it and reset the retry counter.
- If `status != None`, raise an error in `errorHandler_` and clear the pending entry.

### What the Application Should Do

- For non-critical flows (`Telemetry`, `Weights`), you can omit ACK.
- For critical commands (`Command`, `ConfigPush`), ACK is mandatory. If it does not arrive, the device is probably offline.

---

## Fragmentation: ConfigPush

A menu JSON config up to ~3 KB does not fit into one frame (maximum 200 bytes). It is split into chunks via `ConfigChunkPayload`.

### `ConfigChunkPayload` Structure

See [04-binary-structures.md](04-binary-structures.md#configchunkpayload-0x30-up-to-200-bytes):

```
┌─────────────────────────────┐
│ transferId   (uint16 LE, 2) │  Transfer ID
│ totalSize    (uint16 LE, 2) │  Full JSON size (only when chunkIndex == 0)
│ chunkIndex   (uint16 LE, 2) │  0, 1, 2, ...
│ data[0..193]                │  JSON fragment (<=194 bytes)
└─────────────────────────────┘
```

### Fragmentation Flags

| Situation | Frame header flags |
|-----------|--------------------|
| JSON <= 194 bytes, single frame | `FLAG_LAST_FRAGMENT` |
| First fragment out of several | `FLAG_FRAGMENTED` |
| Intermediate fragment | `FLAG_FRAGMENTED` |
| Last fragment | `FLAG_LAST_FRAGMENT` |

Plus the usual `FLAG_ACK_REQUIRED` - the receiver acknowledges each fragment **separately** with `ConfigAck`.

### Reception-Side Assembly Protocol

1. Receive a frame with `FLAG_FRAGMENTED` or `FLAG_LAST_FRAGMENT` - this is a config fragment.
2. If `chunkIndex == 0`:
   - Store `transferId` and `totalSize`.
   - Initialize a buffer with size `totalSize`.
3. Copy `data` into the buffer at offset `chunkIndex * CONFIG_CHUNK_DATA_SIZE` (194 bytes).
4. Send `ConfigAck` with the SEQ number of the original frame.
5. If `FLAG_LAST_FRAGMENT` is set, assembly is complete; parse the JSON from the buffer.
6. If a fragment with a different `transferId` arrives before completion, drop the current transfer.

### Limits

- `CONFIG_CHUNK_DATA_SIZE = 194` bytes per fragment.
- In `CommandHandler`, outgoing `ConfigPush` fragmentation (from LINK -> MCU when `set`/`invoke` carries a large JSON) is not implemented in the reference firmware - small `set`/`invoke` payloads fit into a single frame. This is a [known limitation](../03-uart/03-message-types.md).

---

## Fragmentation: RfidReadData / RfidWriteData

A different fragmentation format, for binary tag data.

- Each fragment is a full `RfidDataPayload` (199 bytes), containing `readerId`, `unitId`, `tag` (for validation), and 163 bytes of data.
- The fragment count is **variable** — `ceil(readSize / 163)`, depending on the chip user-memory size (144 B to 888 B).
- `FLAG_FRAGMENTED` / `FLAG_LAST_FRAGMENT` behave the same way as for `ConfigPush`. Assembly is by `FLAG_LAST_FRAGMENT`, not by counter.

**Writing (`RfidWriteData`, 0x1B)** uses **stop-and-wait ACK flow control**: every fragment carries `FLAG_ACK_REQUIRED`, the sender (LINK) does not send the next one until it receives a `CommandAck`. The reason is that the receiver (RP2040) simultaneously services PID/HX711/PN5180, and without pacing the hardware UART FIFO (32 B) overflows. The approach follows XMODEM stop-and-wait and CAN-TP (ISO 15765-2) STmin. See [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## Idempotency and Deduplication

Sender retry means that **the same frame with the same SEQ may arrive at the receiver twice** (if the first ACK was lost). That is a property of any stop-and-wait protocol.

**The reference `UartBridge` does not do SEQ-based deduplication.** If the receiver processed the request, sent ACK, but the ACK was lost, the retry will arrive again and be processed once more. For idempotent operations, this is harmless:

- `Telemetry`, `Weights`, `Status`, `Heartbeat`, `Rfid` - overwriting the current data; the repeat does no harm.
- `Error`, `Log` - diagnostics; the repeat only duplicates the record, but does not break the logic.

For **non-idempotent commands** (`Command` with `Start`, `Stop`, `ClearErrors`), the application should either:

1. Accept that the repeated command is harmless (`Start` is idempotent - starting something that is already running does not break the logic).
2. Or store the last handled SEQ on the receiver side and ignore repeats.

In most iDryer scenarios this is not a problem: commands from the cloud are rare and manual, so a duplicate `Start` event is visible at the UI level. But if you are building critical logic (for example, heating to a target time) - add a `lastHandledSeq == seq` check.

## Inter-Byte Parser Timeout

In the reference `UartBridge`, parser reset happens **only on error** (`CrcMismatch`, `InvalidPayload`, `UnknownMessage`). There is no reset on a long gap in the byte stream. That means: if the sender stops in the middle of a frame, the parser will remain in `Header`/`Payload`/`Crc` state until it receives enough "continuation" bytes - which may already belong to the next frame.

If you are writing your own parser, **add** a reset to `WaitSof` if there are no bytes for more than about 50 ms after the start of a frame. This protects against getting stuck in the middle.

## Alignment and `_pad` Fields

All `_pad*` fields in the structures (`HelloPayload._pad1`, `StatusEntry._pad`, `RfidPayload._pad`, ...) are alignment bytes. **The sender must fill them with zeros.** This guarantees:

- CRC is calculated deterministically (stack garbage does not affect the result);
- the receiver can use these fields in the future without breaking backward compatibility.

In the library structures, when initialized with `= {}`, `_pad` is automatically zeroed. If you build the frame by hand, check it.

---

## Rules for a Custom Parser

If you are not using `UartBridge`, follow these rules:

1. Increment **SEQ** for every sent frame, including ACK, Heartbeat, and Error.
2. **FLAG_IS_ACK** is for the ACK frame. The receiver uses it to distinguish "a new frame" from "an acknowledgment of my previous frame".
3. In the ACK frame, the `sequence` in the header is the number of the frame being acknowledged, **not** the sender's next sequence.
4. Send the **ACK as fast as possible**, without waiting for payload processing to finish. ACK first, then processing.
5. **Pending queue** - only one pending frame at a time. If you sent something with `ACK_REQUIRED` and are waiting for ACK, do not send another ACK-requiring frame until the first one is acknowledged. Frames that do not require ACK (Heartbeat, Telemetry without ACK) are allowed.
6. **Inter-byte timeout** - add a parser reset if the gap exceeds 50 ms in the middle of a frame.
7. **If `payloadLength > 200`** - immediately drop the frame and send `Error{InvalidPayload}`.

---

## What Next

- [06-examples.md](06-examples.md) - live hex dumps of every frame type with CRC breakdown.
- [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md) - full fragmented-config scenario.
