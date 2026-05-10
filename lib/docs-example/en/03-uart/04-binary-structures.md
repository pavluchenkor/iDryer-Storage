# Binary Payload Format (Normative Reference)

Exhaustive description of every payload structure: field by field, byte by byte. This is normative for parsers. If your implementation does not match this page, check `src/uart/uart_protocol.h` as the source of truth and open an issue in the docs.

!!! note "General Rules"
    - All structures use `#pragma pack(1)`, with no alignment and contiguous bytes.
    - All multi-byte numbers are **little-endian**.
    - `static_assert` in `uart_protocol.h` guarantees the stated sizes at compile time.

---

## Frame Header (FrameHeader) - 6 bytes

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `sof` | `0xAA` |
| 1 | 1 | uint8 | `version` | `0x01` |
| 2 | 1 | uint8 | `flags` | see [02-frame-and-crc.md](02-frame-and-crc.md) |
| 3 | 1 | uint8 (`MessageKind`) | `kind` | message type |
| 4 | 1 | uint8 | `sequence` | counter 0-255 |
| 5 | 1 | uint8 | `payloadLength` | payload length, 0-200 |

After the payload come 2 bytes of CRC16 little-endian.

---

## HelloPayload (0x01) - 86 bytes

Direction: MCU -> LINK; LINK -> MCU trigger (`role = 0xFF`, no payload).

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 (`Role`) | `role` | `0x01`=MCU, `0x02`=ESP (reserved), `0xFF`=HelloRequest |
| 1 | 1 | uint8 (`DeviceType`) | `deviceType` | see the table below; `0`=legacy -> portal treats it as Dryer |
| 2 | 2 | - | `_pad1[2]` | alignment |
| 4 | 4 | uint32 LE | `firmwareVersion` | `(MAJOR<<16) \| (MINOR<<8) \| PATCH` |
| 8 | 4 | uint32 LE | `workTimeCounter` | operating time, seconds |
| 12 | 8 | char[8] | `hardwareVersion` | ASCII string, null-padded |
| 20 | 1 | uint8 | `unitsCount` | number of chambers 0-4 |
| 21 | 48 | `UnitConfig[4]` | `units` | 4 x 12 bytes; `units[unitsCount..3]` are zero-filled |
| 69 | 17 | char[17] | `mcuSerial` | 16 hex characters of the serial number + `\0` |

### `DeviceType`

| Value | Name | Description |
|-------|------|-------------|
| `0x00` | `Unknown` | Legacy / field not set. Portal treats it as Dryer. |
| `0x01` | `Dryer` | iDryer; chamber count is taken from `unitsCount` |
| `0x02` | `Heater` | iHeater |
| `0x03` | `Telemetry` | Telemetry module |
| `0x04` | `Link` | Generic LINK (standalone, without an MCU companion) |
| `0x05` | `LinkII` | Specialized LINK for iHeater |
| `0x06..0xFF` | - | Reserved |

### `UnitConfig` (12 bytes, nested in `HelloPayload.units`)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `unitId` | 0-3 (corresponds to U1..U4 in MQTT) |
| 1 | 1 | - | `_pad1` | alignment for `capabilities` |
| 2 | 2 | uint16 LE | `capabilities` | hardware bit flags (see below) |
| 4 | 4 | uint8[4] | `scales` | scale sensor indices 0-3, `0xFF` = unused |
| 8 | 4 | uint8[4] | `rfid` | RFID reader indices 0-3, `0xFF` = unused |

### `capabilities` Flags

| Bit | Mask | Constant | Description |
|-----|------|----------|-------------|
| 0 | `0x0001` | `HEATER` | Heater present |
| 1 | `0x0002` | `FAN` | Fan present |
| 2 | `0x0004` | `SERVO` | Damper servo present |
| 3 | `0x0008` | `RH_AIR_SENSOR` | Air humidity sensor |
| 4 | `0x0010` | `TEMP_AIR_SENSOR` | Air temperature sensor |
| 5 | `0x0020` | `TEMP_HEATER_SENSOR` | Heater temperature sensor |
| 6-15 | - | - | Reserved |

---

## HelloAckPayload (0x02) - 37 bytes

Direction: LINK -> MCU.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 | uint32 LE | `ipAddress` | IP in little-endian; `0` = no connection |
| 4 | 33 | char[33] | `ssid` | WiFi name, null-terminated; `""` = no connection |

Example: IP `192.168.1.5` in bytes (little-endian) is `05 01 A8 C0`.

---

## TelemetryPayload (0x10) - 29 bytes

Direction: MCU -> LINK. Fixed length: `1 + 4×7 = 29`, regardless of `unitsCount` - fill unused elements with zeros.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `count` | Actual number of populated entries (1-4) |
| 1 | 4×7 | `TelemetryEntry[4]` | `units` | Array of unit data |

### `TelemetryEntry` (7 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `unitId` | 0-3 |
| 1 | 2 | int16 LE | `temperatureC10` | Temperature x 10 (`553` -> 55.3 C) |
| 3 | 2 | uint16 LE | `humidityPct10` | Humidity x 10 (`452` -> 45.2 %) |
| 5 | 1 | uint8 | `heaterPowerPct` | Power 0-100 |
| 6 | 1 | uint8 | `fanOn` | 0/1 -> false/true |

---

## WeightsPayload (0x12) - 17 bytes

Direction: MCU -> LINK. Fixed length.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `count` | 1-4 |
| 1 | 4×4 | `WeightEntry[4]` | `weights` | Sensor array |

### `WeightEntry` (4 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `sensorId` | 0-3 (W1=0 ... W4=3) |
| 1 | 1 | uint8 | `unitId` | 0-3 - which chamber it is bound to |
| 2 | 2 | uint16 LE | `weightGramsC10` | Weight x 10 (`1234` -> 123.4 g), up to 5000 |

---

## StatusPayload (0x13) - 133 bytes

Direction: MCU -> LINK. Fixed length: `1 + 4×32 + 4 = 133`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `count` | 1-4 |
| 1 | 4×32 | `StatusEntry[4]` | `units` | Unit statuses |
| 129 | 4 | uint32 LE | `uptime` | Device uptime, seconds |

### `StatusEntry` (32 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `unitId` | 0-3 |
| 1 | 1 | uint8 (`DryerMode`) | `mode` | 0=Idle, 1=Drying, 2=Storage, 3=Profile, 4=Fault |
| 2 | 4 | uint32 LE | `sessionNum` | Session number; 0 for Idle/Fault |
| 6 | 2 | int16 LE | `targetTempC10` | Target temperature x 10 |
| 8 | 2 | uint16 LE | `targetHumidityPct` | Target humidity % (0 = unused) |
| 10 | 2 | uint16 LE | `durationMinutes` | Duration, minutes (0 = infinite/Storage) |
| 12 | 4 | uint32 LE | `elapsedSeconds` | Seconds since the current session started |
| 16 | 4 | uint32 LE | `stageElapsedSeconds` | Seconds in the current stage (Profile) |
| 20 | 4 | uint32 LE | `stageRemainingSeconds` | Seconds until the end of the stage (Profile) |
| 24 | 4 | uint32 LE | `totalRemainingSeconds` | Seconds until the end of the program |
| 28 | 1 | uint8 | `currentStage` | Current stage index (Profile, 0-based) |
| 29 | 1 | uint8 | `totalStages` | Total number of stages (Profile) |
| 30 | 1 | uint8 (`StagePhase`) | `stagePhase` | 0=Ramp, 1=Hold |
| 31 | 1 | - | `_pad` | Alignment |

---

## RfidPayload (0x14) - 37 bytes

Direction: MCU -> LINK.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 (`RfidEvent`) | `event` | 1=TagDetected, 2=TagRemoved |
| 1 | 1 | uint8 | `readerId` | 0-3 (R1..R4) |
| 2 | 32 | char[32] | `tag` | Tag HEX ID, null-terminated; empty for TagRemoved |
| 34 | 1 | uint8 | `unitId` | 0-3 |
| 35 | 2 | - | `_pad[2]` | Alignment |

---

## CommandPayload (0x20) - 13 bytes

Direction: LINK -> MCU. **Shares kind `0x20` with `ProfilePayload` (64 bytes)** - the receiver distinguishes the format by `payloadLength`.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 (`CommandCode`) | `command` | see the table below |
| 1 | 1 | uint8 | `targetState` | For Start: `DryerMode` (1=Drying, 2=Storage, 3=Profile) |
| 2 | 1 | uint8 | `unitId` | 0-3 or `0xFF` = all units |
| 3 | 2 | - | `reserved[2]` | Reserved |
| 5 | 4 | uint32 LE | `arg0` | For Start: target temperature x 10 |
| 9 | 4 | uint32 LE | `arg1` | For Start(Drying): minutes; for Start(Storage): humidity % |

### `CommandCode`

| Code | Name | Description |
|------|------|-------------|
| `0x01` | `Start` | Start the mode (see arg0/arg1/targetState) |
| `0x02` | `Stop` | Stop the unit |
| `0x03` | `Find` | Locate (blink) |
| `0x05` | `GetConfig` | Request the full JSON config |
| `0x06` | `SetConfig` | Apply settings from JSON |
| `0x07` | `ReadRfid` | Read OpenPrintTag from the tag |
| `0x08` | `WriteRfid` | Write a binary payload to the tag. Arms staging on the MCU to receive subsequent `RfidWriteData` (0x1B) fragments. `arg0 = size in bytes`, `arg1 = verify mode` (0=none, 1=header32, 2=full) |
| `0x10` | `ResetFault` | Clear the unit fault |
| `0x11` | `WifiStatus` | Request IP from LINK (MCU -> LINK) |
| `0x12` | `ClearErrors` | Clear the EEPROM error log |

The `0xFF` value in `unitId` is treated as "all units" for commands where that makes sense (Stop, ClearErrors).

---

## ProfilePayload (inside Command 0x20) - 64 bytes

Direction: LINK -> MCU. It differs from `CommandPayload` by payload size.

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 1 | uint8 | `unitId` | 0-3 |
| 1 | 1 | uint8 | `totalStages` | 1-10 |
| 2 | 1 | uint8 | `startStage` | Start index (0-based) |
| 3 | 1 | - | `_pad` | Alignment |
| 4 | 60 | `ProfileStage[10]` | `stages` | Up to 10 stages, 6 bytes each |

### `ProfileStage` (6 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2 | uint16 LE | `temp` | Temperature x 10 (60 C -> 600) |
| 2 | 2 | uint16 LE | `ramp` | Ramp time, seconds (0 = "forced heating", see [../06-flows/04-profile-mode.md](../06-flows/04-profile-mode.md)) |
| 4 | 2 | uint16 LE | `hold` | Hold time, seconds |

---

## ConfigChunkPayload (0x30) - up to 200 bytes

Direction: both directions. Carries a JSON fragment. Sizes:

- Header `ConfigChunkHeader` = 6 bytes.
- Data `data` = up to 194 bytes (`MAX_PAYLOAD_SIZE - 6`).

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 2 | uint16 LE | `transferId` | Transfer ID (do not mix different transfers) |
| 2 | 2 | uint16 LE | `totalSize` | Full JSON size (only in the first fragment, `chunkIndex == 0`) |
| 4 | 2 | uint16 LE | `chunkIndex` | Fragment index: 0, 1, ... N-1 |
| 6 | <=194 | bytes | `data` | JSON data |

**Fragmentation:**

- If JSON <= 194 bytes: one frame with `FLAG_LAST_FRAGMENT`.
- If JSON > 194 bytes: intermediate frames use `FLAG_FRAGMENTED`, and the last one uses `FLAG_LAST_FRAGMENT`.

Details: [05-ack-retry.md](05-ack-retry.md).

**JSON formats in `data`:**

- MCU -> LINK, full config: `{"v":8,"units":3,"active":0,"lang":"en","menu":[...]}`
- MCU -> LINK, delta: `{"d":{"3":[55,60,55],"81":3}}`
- LINK -> MCU, set: `{"cmd":"set","id":3,"unit":0,"val":55}`
- LINK -> MCU, invoke: `{"cmd":"invoke","id":5}`

---

## HeartbeatPayload (0x40) - 9 bytes

Direction: both directions.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 4 | uint32 LE | `uptimeSeconds` | Sender uptime, seconds |
| 4 | 2 | int16 LE | `wifiRssiDbm` | See explanation below |
| 6 | 2 | uint16 LE | `errorsSinceBoot` | Sender error counter (semantics belong to the sender) |
| 8 | 1 | uint8 (`LinkCloudState`) | `cloudState` | Cloud state - significant **only in LINK->MCU** |

**`wifiRssiDbm` in both directions:**

- **LINK -> MCU:** WiFi RSSI in dBm. Typically negative (`-40...-90`). The MCU uses the value to show signal strength in the menu/display.
- **MCU -> LINK:** the field is **overloaded**: LINK has no useful WiFi number coming from the MCU, so the reference MCU firmware writes **MCU temperature x 10** there (int16, positive number in degrees x 10). LINK does not forward that value anywhere in MQTT - it only uses it if it wants it for its own logs.

!!! warning "Contract Ambiguity"
    This "dual use" of the field is historical and not ideal. If you are building your own MCU and you need to pass controller temperature to LINK explicitly, rely on this only for debugging. The stable path is to publish your own values through `Log` (0x60) or through your own fields in the `info` JSON.

### `LinkCloudState`

| Value | Name |
|-------|------|
| `0` | `Idle` |
| `1` | `WifiConnecting` |
| `2` | `Provisioning` |
| `3` | `Registering` |
| `4` | `AwaitingClaim` |
| `5` | `Ready` |
| `6` | `MqttConnecting` |
| `7` | `Online` |

!!! note "About `errorsSinceBoot` and logs"
    The semantics of `errorsSinceBoot` are defined by the **sender** and do not have to be identical between LINK and MCU. The reference LINK firmware writes the UART-link failure counter there (ACK with error, frame `Error` received), not the MCU application error counter. The value from an incoming Heartbeat is **not** forwarded into MQTT.
    For monitoring application errors, use the `Log` frame (0x60) and the MQTT `events` topic.

---

## LogPayload (0x60) - 164 bytes

Direction: MCU -> LINK. Structured application event.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 10 | char[10] | `severity` | `"critical"`, `"error"`, `"warning"`, `"info"` |
| 10 | 20 | char[20] | `source` | `"THERMISTOR"`, `"HEATER"`, `"SHT"`, ... |
| 30 | 32 | char[32] | `event` | `"SENSOR_SHORT"`, `"OVER_MAX"`, `"NO_RESPONSE"`, ... |
| 62 | 100 | char[100] | `message` | Human-readable message |
| 162 | 1 | uint8 | `unitId` | 0-3 |
| 163 | 1 | - | `_pad` | Alignment |

String fields are C strings, and the tail after `\0` is zero-filled.

!!! note "Publishing to MQTT"
    Publishing `Log` to the MQTT `events` topic does **not** happen automatically in the library. Application code in the LINK firmware does that via `setLogHandler()` + `MqttClient::publishEvent()`. The reference Link firmware is wired that way - see `IdryerDevice::handleLog`.

---

## AckPayload (0x11 / 0x21 / 0x31) - 2 bytes

Direction: opposite to the acknowledged frame.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `ackSequence` | `SEQ` of the acknowledged frame |
| 1 | 1 | uint8 (`ErrorCode`) | `status` | `0` = OK |

Note that the **header** of the ACK frame also carries `sequence` equal to the number of the acknowledged frame (not its own). This duplication makes matching easier.

---

## ErrorPayload (0x50) - 4 bytes

Direction: both directions. Sent on parser or validation failure.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`ErrorCode`) | `code` | see the table below |
| 1 | 1 | uint8 | `lastSequence` | SEQ of the frame that caused the error |
| 2 | 2 | uint16 LE | `detail` | Additional information (expected/actual length, and so on) |

### `ErrorCode`

| Code | Name | Description |
|------|------|-------------|
| `0x00` | `None` | OK |
| `0x01` | `CrcMismatch` | CRC did not match |
| `0x02` | `UnknownMessage` | Unknown `MessageKind` |
| `0x03` | `InvalidPayload` | Invalid payload size/format |
| `0x04` | `Busy` | Device is busy |
| `0x05` | `Timeout` | ACK not received in 700 ms x 3 |
| `0x06` | `SequenceMismatch` | Unexpected SEQ |

---

## ClaimStatusPayload (0x71) - 18 bytes

Direction: LINK -> MCU.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`ClaimingStatus`) | `status` | 0=Idle, 1=Provisioning, 2=WaitingClaim, 3=Claimed, 4=Error |
| 1 | 9 | char[9] | `pin` | 8-digit PIN + `\0`; empty outside `WaitingClaim` |
| 10 | 4 | uint32 LE | `expiresAt` | Unix timestamp when the PIN expires |
| 14 | 4 | uint32 LE | `remainingSeconds` | Seconds remaining until PIN expiration |

---

## ClaimCompletePayload (0x72) - 38 bytes

Direction: LINK -> MCU.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `success` | 1 = success, 0 = error/timeout |
| 1 | 37 | char[37] | `deviceId` | Device UUID (display only) |

---

## WsEnablePayload (0x73) - 4 bytes

Direction: MCU -> LINK.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `enable` | 1 = enable, 0 = disable |
| 1 | 1 | - | `reserved` | - |
| 2 | 2 | uint16 LE | `pin` | PIN 0-9999 (4 digits, generated by MCU) |

---

## WsStatusPayload (0x74) - 6 bytes

Direction: LINK -> MCU.

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 (`WsState`) | `state` | 0=Disabled, 1=Listening, 2=Connected |
| 1 | 2 | uint16 LE | `pin` | PIN 0-9999 |
| 3 | 1 | uint8 | `pairedCount` | Number of paired clients 0-5 |
| 4 | 1 | uint8 | `maxClients` | Maximum (5) |
| 5 | 1 | - | `reserved` | - |

---

## RfidDataPayload (0x1A / 0x1B) - 199 bytes

Direction: 0x1A - MCU->LINK; 0x1B - LINK->MCU. Carries a tag-data fragment (888 bytes total / 163 = 6 fragments).

| Offset | Size | Type | Field | Description |
|--------|------|------|------|----------|
| 0 | 1 | uint8 | `readerId` | 0-3 |
| 1 | 1 | uint8 | `unitId` | 0-3 |
| 2 | 32 | char[32] | `tag` | Tag HEX ID (for validating the target tag) |
| 34 | 163 | bytes | `fragment` | Binary data fragment |
| 197 | 2 | - | `_pad[2]` | Alignment |

!!! note "Status"
    Implemented end-to-end (`commands/read_rfid`, `commands/write_rfid`). Writing uses stop-and-wait ACK flow control — every fragment carries `FLAG_ACK_REQUIRED`. See [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## What Next

- [05-ack-retry.md](05-ack-retry.md) - how acknowledgments, retry, and fragmentation work.
- [06-examples.md](06-examples.md) - live hex dumps of every frame type.
