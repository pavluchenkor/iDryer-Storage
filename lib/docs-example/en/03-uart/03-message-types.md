# Message Types (MessageKind)

An exhaustive list of all UART frame types, with direction, expected payload length, and status in the current library release. If you are working on the receiver side, this is your checklist.

!!! note "Source of Truth"
    `enum class MessageKind` in `src/uart/uart_protocol.h`. Any mismatch with this table is a documentation bug.

---

## Legend

- **Direction** - primary direction. ACK frames may go the other way - see the corresponding column.
- **LEN** - expected payload length. "F" means fragmented.
- **ACK** - whether acknowledgment is required. "Opt." means the sender may choose, via `ackRequired` in the API.
- **Status** - maturity in the release. ✅ works; ⚠️ partial; ❌ contract only.

---

## Full Table

| Code | Name | Direction | LEN | ACK | Status | Purpose |
|------|------|-----------|-----|-----|--------|---------|
| `0x01` | `Hello` | MCU -> LINK | 86 | Opt. | ✅ | MCU greeting, role, version, units, mcuSerial |
| `0x01` | `Hello` (HelloRequest) | LINK -> MCU | 86 | - | ✅ | Trigger with `role=0xFF`, asks the MCU to send its Hello |
| `0x02` | `HelloAck` | LINK -> MCU | 37 | - | ✅ | Response: IP + SSID |
| `0x10` | `Telemetry` | MCU -> LINK | 29 | Opt. | ✅ | Temperature, humidity, heater, fan |
| `0x11` | `TelemetryAck` | LINK -> MCU | 2 | - | ✅ | Telemetry acknowledgment |
| `0x12` | `Weights` | MCU -> LINK | 17 | Opt. | ✅ | Filament weights, up to 4 sensors |
| `0x13` | `Status` | MCU -> LINK | 133 | Opt. | ✅ | Modes, timers, sessions |
| `0x14` | `Rfid` | MCU -> LINK | 37 | Opt. | ✅ | `tag_detected` / `tag_removed` events |
| `0x1A` | `RfidReadData` | MCU -> LINK | F | Opt. | ✅ | Tag data for reading. Fragmented at 163 B, assembled by `FLAG_LAST_FRAGMENT` |
| `0x1B` | `RfidWriteData` | LINK -> MCU | F | Yes | ✅ | Fragment of data to write. Stop-and-wait ACK flow control |
| `0x20` | `Command` | LINK -> MCU | 13 or 64 | Yes | ✅ | `Start`/`Stop`/... command (13) **or** `ProfilePayload` (64) |
| `0x21` | `CommandAck` | MCU -> LINK | 2 | - | ✅ | `Command` acknowledgment |
| `0x30` | `ConfigPush` | Both directions | <=200, F | Yes | ✅ | JSON menu config (full or delta), fragmented |
| `0x31` | `ConfigAck` | Both directions | 2 | - | ✅ | `ConfigPush` acknowledgment |
| `0x40` | `Heartbeat` | Both directions | 9 | No | ✅ | Periodic "alive", uptime, RSSI, cloudState |
| `0x50` | `Error` | Both directions | 4 | No | ✅ | Error code, bad frame SEQ, details |
| `0x60` | `Log` | MCU -> LINK | 164 | No | ✅ | Structured application event |
| `0x70` | `ClaimStart` | MCU -> LINK | 0 | Yes | ✅ | Request to start claiming |
| `0x71` | `ClaimStatus` | LINK -> MCU | 18 | No | ✅ | PIN and time remaining |
| `0x72` | `ClaimComplete` | LINK -> MCU | 38 | No | ✅ | Reports claim success/failure, deviceId |
| `0x73` | `WsEnable` | MCU -> LINK | 4 | No | ✅ | Enable/disable local WebSocket |
| `0x74` | `WsStatus` | LINK -> MCU | 6 | No | ✅ | WebSocket server status |
| `0x75` | `WsResetClients` | MCU -> LINK | 0 | No | ✅ | Reset WS client bindings |
| `0x76` | `WsStatusRequest` | MCU -> LINK | 0 | No | ✅ | Request current WS status |

Reserved codes `0x03-0x0F`, `0x15-0x19`, `0x77-0xFF` are not used.

---

## Categories

### Handshake (0x01-0x02)

Connection initialization. See the "Who Speaks First" section in [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md).

### Telemetry and Status (0x10-0x14, 0x1A, 0x1B)

Periodic data from the MCU. Expected frequency:

- `Telemetry` - once per second (active mode) / once per 15 seconds (idle). Constants `TELEMETRY_ACTIVE_INTERVAL_MS`, `TELEMETRY_IDLE_INTERVAL_MS`.
- `Status` - on event (mode change), not on a timer.
- `Weights` - when weight changes beyond a threshold or periodically.
- `Rfid` - on event (`tag_detected` / `tag_removed`), plus cache on connect.
- `RfidReadData` - response to `commands/read_rfid`, variable number of fragments based on chip user-memory size.
- `RfidWriteData` - fragments for writing, sent after `Command WriteRfid` (0x08). Each fragment carries `FLAG_ACK_REQUIRED` — stop-and-wait flow control. See [../07-features/01-rfid.md](../07-features/01-rfid.md).

### Commands (0x20-0x21)

Cloud control. `Command` may carry:

- `CommandPayload` (13 bytes) - simple command (`Start`, `Stop`, `Find`, `GetConfig`, `ReadRfid`, `ClearErrors`, and so on);
- `ProfilePayload` (64 bytes) - start a drying profile with stages.

**Format discrimination:** by payload length. The receiver must look at `payloadLength`, not `targetState` or any other field.

### Remote Config (0x30-0x31)

Transfer of the menu JSON config between MCU and LINK. Fragmented into 194-byte chunks. Details: [05-ack-retry.md](05-ack-retry.md) and [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md).

### Service Messages (0x40-0x60)

- `Heartbeat` - every 5 seconds. In the LINK -> MCU frame it carries `cloudState` (cloud connection state).
- `Error` - on parser or validation failure.
- `Log` - structured application events (critical hardware errors, warnings). In the reference LINK firmware they are published to the MQTT topic `events` (if application code is subscribed via `setLogHandler`).

### Claiming (0x70-0x72)

Device claiming to the user account through the portal. Initiated by the MCU based on user action (menu button). Details: [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).

### WebSocket Local (0x73-0x76)

Local control channel without cloud. Details: [../07-features/02-websocket-local.md](../07-features/02-websocket-local.md).

---

## Feature Statuses - Details

### ⚠️ Partial

- **`ConfigPush` fragmentation from MQTT for large `set`/`invoke` calls** - the code emits "fragmentation not implemented" when trying to send JSON larger than `CONFIG_CHUNK_DATA_SIZE`. Fragmentation works for configs coming from the MCU.

### ℹ️ RFID specifics worth knowing

- **`RfidWriteData` (0x1B)** uses **stop-and-wait ACK flow control**: every fragment carries `FLAG_ACK_REQUIRED`, LINK does not send the next one until it receives a `CommandAck`. ACK timeout 200 ms, up to 3 retries, then abort with a log in `events`. See [../07-features/01-rfid.md](../07-features/01-rfid.md).
- **`RfidReadData` (0x1A)** - variable number of fragments, assembled by `FLAG_LAST_FRAGMENT`, not by counter.

---

## What Next

- [04-binary-structures.md](04-binary-structures.md) - detailed offsets of every payload structure.
- [05-ack-retry.md](05-ack-retry.md) - ACK, retry, and fragmentation mechanics.
- [06-examples.md](06-examples.md) - live hex dumps.
