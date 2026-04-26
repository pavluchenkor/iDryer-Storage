# MQTT: commands from cloud to device

The full contract for `idryer/<serial>/commands/<name>` topics. For each command - JSON format, required fields, and what happens next (UART frame, local action).

!!! note "Source of truth"
    `src/cloud/command_handler.cpp`, `handleMqttCommand`, and the `handleStart`, `handleStorage`, `handleProfile`, `handleStop`, `handleFind`, `handleSet`, `handleInvoke`, `handleGetConfig`, `handleReadRfid`, `handleWriteRfid`, `handleClearErrors`, `handlePing` handlers.

---

## General rules

1. **All commands** arrive in the `idryer/<serial>/commands/<name>` topic. QoS 1.
2. **Body is JSON.** An empty payload (`""`) is allowed for commands without parameters - the JSON parser accepts an empty object.
3. **The `unitId` field** is always a string in the form `"U1"`...`"U4"`. If it is missing or malformed, `CommandHandler` treats it as "all chambers" (UART `unitId = 0xFF`).
4. **Optional `timestamp` field** (ISO 8601) in the JSON root - if `setTimeSyncCallback()` is set, it will be called to synchronize the device RTC.
5. **Commands that change MCU state** go over UART with `ackRequired = true`. The MCU responds with `CommandAck`.

---

## `drying` - start drying / manual chamber temperature control (iHeater)

### New format (recommended)

```json
{
  "unitId": "U1",
  "params": {
    "temperature": 55,
    "duration": 240
  }
}
```

!!! note "For `deviceType: heater` / `link_ii` - the same channel"
    The portal also uses this command as a remote dashboard tile for manually setting the temperature of the iHeater **heater chamber**. The field semantics are the same:
    - `temperature` - target chamber temperature in °C.
    - `duration: 0` - indefinite (runs until `stop` arrives or `VIRTUAL_CHAMBER.target` overrides it).
    - `duration: N` - automatic shutdown after N minutes.

    Priority between `commands/drying`, `VIRTUAL_CHAMBER.target` from Klipper, and the local menu is iHeater-MCU firmware business logic, not protocol logic. Recommended order: Moonraker > drying > menu.

### Legacy format (without `params`)

```json
{
  "unitId": "U1",
  "targetTemperature": 55.0,
  "durationMinutes": 240
}
```

| Field | Type | Default value | Purpose |
|------|-----|------------------------|------------|
| `unitId` | string | `"U1"`..`"U4"` or omitted -> all | Chamber |
| `params.temperature` | int (°C) | 55 | Target temperature |
| `params.duration` | int (min) | 240 | Duration |

**UART effect:** `CommandPayload` (13 bytes), `command=Start`, `targetState=DryerMode::Drying(1)`, `arg0=temperature*10`, `arg1=duration`.

---

## `storage` - storage mode

```json
{
  "unitId": "U1",
  "params": {
    "temperature": 40,
    "humidity": 15
  }
}
```

| Field | Type | Default | Purpose |
|------|-----|---------|------------|
| `params.temperature` | int (°C) | 40 | Target temperature |
| `params.humidity` | int (%) | 15 | Target humidity |

**UART effect:** `CommandPayload`, `command=Start`, `targetState=DryerMode::Storage(2)`, `arg0=temperature*10`, `arg1=humidity`.

Storage duration is not specified - the mode runs until `stop` arrives.

---

## `profile` - multi-stage drying

```json
{
  "unitId": "U1",
  "mode": "PROFILE",
  "startStage": 0,
  "stages": [
    { "temp": 60,  "ramp": 300,  "hold": 1800 },
    { "temp": 100, "ramp": 600,  "hold": 6000 },
    { "temp": 70,  "ramp": 600,  "hold": 12000 }
  ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

| Field | Type | Required | Purpose |
|------|-----|-------------|------------|
| `unitId` | string | yes | `"U1"`..`"U4"` |
| `stages[]` | array | yes | 1-10 stages |
| `startStage` | int | no, default 0 | Stage index to start from (0-based) |
| `mode` | string | no, not read | For backend documentation, parser ignores it |

### `stages[i]`

| Field | Type | Unit | Description |
|------|-----|---------|----------|
| `temp` | int or float | °C | Target stage temperature |
| `ramp` | int | seconds | Ramp time; `0` = "forced heating" to the target |
| `hold` | int | seconds | Hold time |

!!! note "More than 10 stages"
    If `stages.length > 10`, LINK truncates to 10, logs a warning, and continues. If the array is missing or has fewer than 1 item, the command is dropped without effect.

!!! warning "startStage out-of-range"
    LINK does **not** validate `startStage` against `stages.length`. The value goes into the UART payload as-is. If you set `startStage=5` for 3 stages, the MCU decides what happens - it may enter Fault, start from stage 0, or react differently. The client must verify `0 <= startStage < stages.length` on its own.

**UART effect:** `ProfilePayload` (64 bytes) inside `MessageKind::Command` (0x20). Payload-length discrimination - [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md#profilepayload-inside-command-0x20-64-bytes).

Stage semantics and PID pseudocode - [../06-flows/04-profile-mode.md](../06-flows/04-profile-mode.md).

---

## `stop` - stop

```json
{ "unitId": "U1" }
```

Without `unitId` -> stop all chambers (`unitId = 0xFF`).

**UART effect:** `CommandPayload`, `command=Stop`, `targetState=Idle`, `arg0=0, arg1=0`.

---

## `find` - blink / local beacon

```json
{ "unitId": "U1" }
```

**UART effect:** `CommandPayload`, `command=Find`.

The effect is implemented in the MCU (usually a few seconds of LED or display blinking).

---

## `get_config` - request full menu config

```json
{}
```

or

```json
{ "unitId": "U1" }
```

**UART effect:** `CommandPayload`, `command=GetConfig`. The MCU sends a fragmented `ConfigPush` with the full menu JSON in response. LINK assembles it and publishes it to `idryer/<serial>/config`.

---

## `set` - change a menu parameter

```json
{ "id": 3, "unit": 0, "val": 55 }
```

| Field | Type | Description |
|------|-----|----------|
| `id` | int | Menu item identifier |
| `unit` | int | Chamber (0-based index); optional, defaults to current |
| `val` | int, float or array | New value |

**UART effect:** JSON `{"cmd":"set","id":3,"unit":0,"val":55}` goes as `ConfigPush` (0x30), not as `CommandPayload`. The MCU applies it, updates the local config, and publishes `config/delta` back.

!!! warning "set/invoke fragmentation is limited"
    In the reference firmware, fragmentation for large `set`/`invoke` JSON from MQTT is **not implemented**. The packet must fit into `CONFIG_CHUNK_DATA_SIZE = 194` bytes after the 6-byte chunk header is added. Large `val` arrays may not fit. See [../03-uart/03-message-types.md](../03-uart/03-message-types.md).

---

## `invoke` - invoke a menu action

```json
{ "id": 5 }
```

| Field | Type | Description |
|------|-----|----------|
| `id` | int | Menu action item identifier (the "execute XY" button) |

**UART effect:** JSON `{"cmd":"invoke","id":5}` goes as `ConfigPush`. The MCU executes the corresponding action.

---

## `read_rfid` - read tag

```json
{ "unitId": "U1" }
```

**UART effect:** `CommandPayload`, `command=ReadRfid`. The MCU reads the tag in the specified chamber and sends the data back via `RfidReadData` (0x1A). LINK assembles the fragments and publishes the base64 buffer to `idryer/<serial>/rfid` (retained).

Details - [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## `write_rfid` - write tag

```json
{
  "unitId": "U1",
  "data": "BASE64_STRING...",
  "verify": "header32"
}
```

Parameters:

- `unitId` — chamber identifier `"U1"`...`"U4"`.
- `data` — base64 string, up to 888 bytes after decoding.
- `verify` — post-write readback mode: `"none"` / `"header32"` (default) / `"full"`. Optional.

**UART effect:** LINK decodes `data`, sends `Command WriteRfid` (0x08, `arg0 = rawLen`, `arg1 = verifyCode`), then `RfidWriteData` (0x1B) fragments of 163 bytes of payload each. Fragment count — `ceil(rawLen / 163)`.

**Flow control:** stop-and-wait ACK. Every frame carries `FLAG_ACK_REQUIRED`, LINK does not send the next one until it receives a `CommandAck`. ACK timeout — 200 ms, up to 3 retries, otherwise the transaction aborts.

Protocol details and limits — [../07-features/01-rfid.md](../07-features/01-rfid.md).

---

## `clear_errors` - clear the error log

```json
{ "unitId": "U1" }
```

Without `unitId` -> clears the log for all chambers.

**UART effect:** `CommandPayload`, `command=ClearErrors (0x12)`.

---

## `link_integration` - external integration settings

Save HA / Bambu / Moonraker credentials in LINK NVS. Discriminated union by `type`.

```json
{ "type": "ha",        "enabled": true, "host": "homeassistant.local", ... }
{ "type": "bambu",     "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..." }
{ "type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125, ... }
```

LINK stores the section in NVS. It starts the client only if `activeIntegration` in the menu points to that type. Details - [../07-features/03-link-integrations-overview.md](../07-features/03-link-integrations-overview.md).

!!! note "Status"
    Design-level. The handler is not implemented in the library.

---

## `bambu_apply` - apply filament settings in Bambu

**Published only for `deviceType == "dryer"` (or legacy/Unknown).** For Heater/LinkII the backend does not send it - they are in Reader mode and read the printer status themselves.

Sent by the backend on `tag_detected`, contains a ready OpenSpool-compatible payload. LINK (if `active == "bambu"` and configured) sends `ams_filament_setting` to the printer.

```json
{
  "trayType": "PLA",
  "colorHex": "FFAABBFF",
  "nozzleTempMin": 209,
  "nozzleTempMax": 231,
  "trayInfoIdx": "GFL99",
  "settingId": "",
  "amsId": null,
  "trayId": null,
  "spoolId": "uuid-...",
  "uid": "AABB1234"
}
```

Details - [../07-features/05-bambu-integration.md](../07-features/05-bambu-integration.md).

!!! note "Status"
    Design-level. The handler is not implemented in the library.

---

## `ping` - liveness check

```json
{}
```

**UART effect:** **none** - the command is logged only on the LINK side. The backend uses it to verify that the device is online and handling incoming MQTT messages.

---

## Summary table

| MQTT command | UART frame | Status |
|--------------|-----------|--------|
| `drying` | `CommandPayload` (Start, Drying) | ✅ |
| `storage` | `CommandPayload` (Start, Storage) | ✅ |
| `profile` | `ProfilePayload` inside `Command` | ✅ |
| `stop` | `CommandPayload` (Stop) | ✅ |
| `find` | `CommandPayload` (Find) | ✅ |
| `get_config` | `CommandPayload` (GetConfig) | ✅ |
| `set` | `ConfigPush` JSON `{"cmd":"set",...}` | ⚠️ fragmentation not implemented |
| `invoke` | `ConfigPush` JSON `{"cmd":"invoke",...}` | ✅ (up to chunk limit) |
| `read_rfid` | `CommandPayload` (ReadRfid) | ✅ |
| `write_rfid` | `CommandPayload` (WriteRfid) + `RfidWriteData` fragments (stop-and-wait ACK) | ✅ |
| `clear_errors` | `CommandPayload` (ClearErrors) | ✅ |
| `ping` | - | ✅ (log only) |
| `link_integration` | NVS write on LINK | ❌ design-level |
| `bambu_apply` | MQTT request to the Bambu printer | ❌ design-level |

---

## Next

- [05-examples.md](05-examples.md) - ready-to-use `mosquitto_pub` commands for each command.
- [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md) - the `set/invoke` + delta cycle in practice.
- [../06-flows/04-profile-mode.md](../06-flows/04-profile-mode.md) - profile drying end-to-end.
