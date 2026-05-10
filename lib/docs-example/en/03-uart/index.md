# UART Protocol

Binary protocol for communication between the MCU (controller) and LINK (network module). This section is the **normative reference**: frame structure, CRC16, message types, byte-for-byte offsets of every payload, ACK/retry/fragmentation, and live hex dumps.

| Document | Contents |
|----------|----------|
| [01-physical-layer.md](01-physical-layer.md) | Physical layer: 115200 8N1, pins, level shifter for 5 V |
| [02-frame-and-crc.md](02-frame-and-crc.md) | Frame structure, CRC-16/CCITT-FALSE, with working C code |
| [03-message-types.md](03-message-types.md) | Table of all MessageKind values with implementation status |
| [04-binary-structures.md](04-binary-structures.md) | Offsets, sizes, and field types of all payloads |
| [05-ack-retry.md](05-ack-retry.md) | ACK, retry with backoff, ConfigPush and RFID fragmentation |
| [06-examples.md](06-examples.md) | **14 live hex dumps** with verified CRCs |

## If You Are Here for the First Time

This is the hardest part for a beginner. If you are shaky on concepts like "little-endian", "`#pragma pack`", or bitwise operations, start with [../01-overview/05-prerequisites.md](../01-overview/05-prerequisites.md). Without that, this section reads like a cipher.

## Reading Order

- Building your own parser / collector from scratch -> read 01 -> 06 in order.
- You already have a frame and need to understand what is inside -> jump straight to [06-examples.md](06-examples.md), then follow the links to the specific structures.
- Working with `UartBridge` from the library -> [03-message-types.md](03-message-types.md) (message list) and [04-binary-structures.md](04-binary-structures.md) (data shapes) are enough; do not touch the implementation.

## What Next

- [../04-mqtt/](../04-mqtt/index.md) - how UART data becomes MQTT messages.
- [../06-flows/](../06-flows/index.md) - end-to-end scenarios where UART frames appear in context.
