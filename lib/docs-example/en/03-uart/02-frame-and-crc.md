# UART Frame Structure and CRC

The unit of exchange on UART is a **frame**. This page is the exhaustive reference: what it consists of, which fields are mandatory, and how to calculate the checksum.

!!! note "Source of Truth"
    Definitions are in `src/uart/uart_protocol.h`, and the CRC implementation is in `src/uart/uart_protocol.cpp`. Everything below has been checked against those files.

!!! warning "Basic Concepts - See prerequisites"
    The following are used heavily: fixed-width types (`uint8_t`, `uint16_t`), **little-endian**, bitwise operations (`<<`, `&`, `|`), `#pragma pack(1)`, and `enum class`. If you are unsure about even one of these, start with [../01-overview/05-prerequisites.md](../01-overview/05-prerequisites.md). Without it, the sections below will read like a cipher.

---

## Frame Layout

```
┌─────┬─────┬──────┬──────┬─────┬─────┬─────────────┬──────────────────────┐
│ SOF │ VER │FLAGS │ KIND │ SEQ │ LEN │   PAYLOAD   │ CRC16 (2B, LE)       │
│ 1B  │ 1B  │  1B  │  1B  │ 1B  │ 1B  │ 0-200 bytes │ least significant byte first │
└─────┴─────┴──────┴──────┴─────┴─────┴─────────────┴──────────────────────┘
◄─────────── 6-byte header ────────────►
```

**LE** means **little-endian**, byte order with the least significant byte first. This applies to **all** multi-byte fields in the protocol, not only CRC. See [prerequisites §3](../01-overview/05-prerequisites.md) for details.

**Total frame length:** `6 + LEN + 2` bytes, from 8 (empty payload) to 208 (maximum 200-byte payload).

---

## Header (6 bytes)

| Offset | Size | Field | Value | Constant |
|--------|------|-------|-------|----------|
| 0 | 1 | `sof` | Always `0xAA` - start byte | `DryerUart::SOF` |
| 1 | 1 | `version` | Always `0x01` | `DryerUart::PROTOCOL_VERSION` |
| 2 | 1 | `flags` | Bit flags (see below) | - |
| 3 | 1 | `kind` | Message type (MessageKind) | `enum class MessageKind` |
| 4 | 1 | `sequence` | Counter 0-255, incremented for each frame | - |
| 5 | 1 | `payloadLength` | Payload length, 0-200 | `MAX_PAYLOAD_SIZE = 200` |

### Flags (byte 2)

| Bit | Mask | Constant | Description |
|-----|------|----------|-------------|
| 0 | `0x01` | `FLAG_ACK_REQUIRED` | Sender expects an ACK |
| 1 | `0x02` | `FLAG_IS_ACK` | This frame is itself an ACK |
| 2 | `0x04` | `FLAG_ERROR` | Payload contains `ErrorPayload` |
| 3 | `0x08` | `FLAG_FRAGMENTED` | Intermediate fragment (not the last one) |
| 4 | `0x10` | `FLAG_LAST_FRAGMENT` | Last fragment (single frame for indivisible messages) |

Flags can be combined. For example, an ACK response to a fragment is `FLAG_IS_ACK | FLAG_LAST_FRAGMENT`.

### `sequence`

Incremented by the **sender** for every outgoing frame, including ACKs. It wraps after 255 -> 0.

In an ACK frame (`FLAG_IS_ACK` = 1), the `sequence` field is the **number of the frame being acknowledged**, not the ACK's own sequence. This lets the receiver match the ACK to the original request.

### `payloadLength`

Payload length in bytes. Every message type has an **expected** length - the receiver validates it and returns `Error{InvalidPayload}` if it does not match. The only exception is `Command` (`0x20`), which allows two sizes: 13 bytes (`CommandPayload`) and 64 bytes (`ProfilePayload`).

---

## Payload (0-200 bytes)

The structure depends on `kind`. The full reference for all structures is in [04-binary-structures.md](04-binary-structures.md).

**General rule:** all structures are packed with `#pragma pack(1)` - no alignment, bytes are contiguous. All multi-byte numbers are **little-endian**.

---

## CRC16-CCITT

### Parameters

| Parameter | Value |
|-----------|-------|
| Polynomial | `0x1021` |
| Initial value | `0xFFFF` |
| Reflect Input | no |
| Reflect Output | no |
| Xor Output | `0x0000` |
| **Standard name** | **CRC-16/CCITT-FALSE** (also CRC-16/IBM-3740) |

!!! warning "Do Not Confuse It with XMODEM"
    Online calculators and libraries often offer several CCITT variants. XMODEM uses **init = `0x0000`**, while ours uses **init = `0xFFFF`**. Check the init value.

### Which bytes it is calculated from

CRC is calculated over **the entire header and the entire payload**, that is `HEADER_SIZE + payloadLength = 6 + LEN` bytes. It **does not include** the CRC itself.

### Writing it into the frame

CRC is written as **two bytes** after the payload in **little-endian**: least significant byte first.

```
... payload[LEN-1] | (crc & 0xFF) | ((crc >> 8) & 0xFF)
```

### C Implementation

This function matches `DryerUart::calculateCrc()` exactly (`src/uart/uart_protocol.cpp`):

```cpp
uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;                          // initial value (init)
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;              // put the current byte into the
                                                    // upper half of the 16-bit word and XOR
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)                       // most significant bit = 1?
                crc = (crc << 1) ^ 0x1021;          // yes - shift left + XOR with the polynomial
            else
                crc <<= 1;                          // no - just shift left
        }
    }
    return crc;
}
```

**What the code does, in simple terms:**

- `0xFFFF` - "all 16 bits set to 1", the initial state.
- `<<= 8` - shift by 8 bits (place the byte into the upper half).
- `& 0x8000` - mask for the "most significant bit of a 16-bit number" (0b1000_0000_0000_0000).
- `^` (XOR) - bitwise exclusive OR.
- Polynomial `0x1021` = CCITT standard, do not change it.

The detail of why this exact sequence of shifts and XORs is used is the **definition** of CRC-16/CCITT. It is enough to copy the function as-is - it works. If you want the theory, search for "CRC polynomial long division".

### Check Values

For an empty array (len=0): `0xFFFF`.

For the Telemetry test frame from the quickstart (35 bytes = 6-byte header + 29-byte payload, **without** the two CRC bytes): **`0xDA10`**.

For the Hello test frame (92 bytes = 6-byte header + 86-byte payload, **without** the two CRC bytes): **`0xE274`**.

Full byte-for-byte dumps of these frames are in [06-examples.md](06-examples.md). If you wrote your own function, run it on these inputs and compare. Match -> CRC is correct.

---

## Frame Lifecycle

### Sender

1. Build the header: `sof = 0xAA`, `version = 1`, `flags` as needed, `kind` = message type, `sequence` = next number, `payloadLength` = payload size.
2. Build the payload (structure depends on the message type).
3. Calculate CRC over `header + payload` (6 + LEN bytes).
4. Write to UART: `header (6) + payload (LEN) + CRC_lo + CRC_hi`.
5. If `FLAG_ACK_REQUIRED` is set, store the frame as pending, wait 700 ms for ACK; if it does not arrive, retry (up to 3 times), see [05-ack-retry.md](05-ack-retry.md).

### Receiver

1. Wait for `0xAA` in the byte stream.
2. Read 6 bytes of header.
3. Validate `version == 1`, `payloadLength <= 200`. Otherwise, signal an error and reset the parser.
4. Read `payloadLength` bytes of payload.
5. Read 2 bytes of CRC (LE).
6. Calculate CRC over `header + payload` using your own implementation. Compare it to the received value.
7. If it does not match, return `Error{CrcMismatch}` and reset the parser.
8. If it matches, run `validateLength(kind, payloadLength)`. If the length does not match for this type, return `Error{InvalidPayload}`.
9. If `FLAG_ACK_REQUIRED` is set, reply immediately with the matching ACK frame.
10. Hand the payload to the handler for that message type.

Reference implementation: `DryerUart::UartBridge::processIncomingByte()` in `src/uart/uart_bridge.cpp`.

---

## Limits

- **Payload > 200 bytes** is fundamentally impossible in a single frame. For large JSON blobs (menu config, ~3 KB), fragmentation is used: `ConfigPush` (`0x30`) with `FLAG_FRAGMENTED` on intermediate fragments and `FLAG_LAST_FRAGMENT` on the last one. Details: [05-ack-retry.md](05-ack-retry.md) and [04-binary-structures.md#configchunkpayload-0x30-up-to-200-bytes](04-binary-structures.md#configchunkpayload-0x30-up-to-200-bytes).
- **RFID data** (888 bytes) is also fragmented (`RfidReadData`/`RfidWriteData`), but the format differs: every fragment has a shared `RfidDataPayload` header. See [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## What Next

- [03-message-types.md](03-message-types.md) - table of all `MessageKind` values.
- [04-binary-structures.md](04-binary-structures.md) - offsets of all payload structures.
- [05-ack-retry.md](05-ack-retry.md) - ACK, retry, fragmentation.
- [06-examples.md](06-examples.md) - live examples with bytes.
