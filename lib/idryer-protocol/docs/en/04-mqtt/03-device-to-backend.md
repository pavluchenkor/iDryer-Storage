# MQTT: JSON from device to cloud

Publication formats for the `idryer/<serial>/...` topics. All examples are real and reflect what `TelemetryPublisher` and `MqttClient` write.

!!! note "Source of truth"
    `src/cloud/telemetry_publisher.cpp` and `src/mqtt/mqtt_client.cpp` (`publishInfo`). Any mismatch is a documentation bug.

---

## `info` (retained)

Static device information. Published **once** after `Online`. Republished only when the configuration changes (reboot with a new `unitsCount`, new firmware version).

```json
{
  "hardwareVersion": "v1.0",
  "firmwareVersion": "1.2.3",
  "workTimeCounter": 360000,
  "unitsCount": 2,
  "mcuSerial": "36B955AB4350FEDC",
  "deviceType": "dryer",
  "units": [
    {
      "unitId": 0,
      "capabilities": {
        "heater": true,
        "fan": true,
        "servo": false,
        "RhAirSensor": true,
        "TempAirSensor": true,
        "TempHeaterSensor": true
      },
      "scales": [0, 1],
      "rfid": [0]
    },
    {
      "unitId": 1,
      "capabilities": {
        "heater": true,
        "fan": true,
        "servo": false,
        "RhAirSensor": true,
        "TempAirSensor": true,
        "TempHeaterSensor": false
      },
      "scales": [2, 3],
      "rfid": [1]
    }
  ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

**Fields:**

| Field | Type | Description |
|------|-----|----------|
| `hardwareVersion` | string | From `HelloPayload.hardwareVersion` |
| `firmwareVersion` | string | Formatted version `"MAJOR.MINOR.PATCH"` |
| `workTimeCounter` | int | Uptime in seconds |
| `unitsCount` | int | 0-4 |
| `mcuSerial` | string | 16 hex characters from the MCU, if provided |
| `deviceType` | string | `"dryer"`, `"heater"`, `"telemetry"`, `"link"`, `"link_ii"`; **absent** on legacy firmware |
| `units[]` | array | Chamber configuration (only the "live" chambers, `unitsCount` items) |
| `units[i].unitId` | int | 0-3 |
| `units[i].capabilities.*` | bool | Hardware flags (heater, fan, servo, RhAirSensor, TempAirSensor, TempHeaterSensor) |
| `units[i].scales[]` | array int | Indices of attached weight sensors (`0xFF` filtered out) |
| `units[i].rfid[]` | array int | Indices of attached RFID readers (`0xFF` filtered out) |
| `timestamp` | ISO 8601 UTC | Publication time |

!!! note "deviceType on legacy firmware"
    If the MCU does not fill `deviceType` in `HelloPayload` (value `0x00`), LINK does not include the field in JSON at all. In that case the portal treats the device as `"dryer"` with `unitsCount` chambers.

!!! warning "unitId in info is a number, not a string"
    In `info`, the `units[].unitId` field is **number 0-3** (the raw value from `UnitConfig.unitId`). In the other topics (`telemetry`, `status`, `weights`, `rfid`, commands), `unitId` is **string `"U1"`...`"U4"`**. This asymmetry is historical; account for it when parsing on the backend.

---

## `telemetry` (QoS 0, not retained)

Current measurements. Published by the application timer (usually every 5 seconds).

```json
{
  "units": [
    {
      "unitId": "U1",
      "temperature": 55.3,
      "humidity": 45.2,
      "heaterPower": 80,
      "fanStatus": true
    }
  ]
}
```

| Field | Type | Note |
|------|-----|------------|
| `units[].unitId` | string | `"U1"`...`"U4"` (index + 1, converted from UART `unitId: 0..3`) |
| `units[].temperature` | float | `temperatureC10 / 10`, 1 decimal place |
| `units[].humidity` | float | `humidityPct10 / 10`, 1 decimal place |
| `units[].heaterPower` | int | 0-100 |
| `units[].fanStatus` | bool | from `fanOn` (0/1) |

The number of elements in `units[]` matches `count` in the UART packet (1-4). If the UART packet has `count=1`, JSON contains one element - a list of populated chambers, not a fixed-length array of 4.

---

## `status` (QoS 1, retained)

Operating mode and timers. Published **on event** (mode change), not on a fixed interval.

### Idle / Fault - short form

```json
{
  "units": [{ "unitId": "U1", "mode": "IDLE" }],
  "uptime": 12345
}
```

### Active mode (Drying / Storage)

```json
{
  "units": [
    {
      "unitId": "U1",
      "mode": "DRYING",
      "sessionNum": 42,
      "target": {
        "temperature": 55.0,
        "duration": 120,
        "humidity": 15
      },
      "totalElapsed": 360,
      "totalRemaining": 6840
    }
  ],
  "uptime": 12345
}
```

`target.humidity` is added only if `targetHumidityPct > 0` (relevant for STORAGE).

### Profile mode

```json
{
  "units": [
    {
      "unitId": "U1",
      "mode": "PROFILE",
      "sessionNum": 42,
      "target": { "temperature": 80.0, "duration": 0 },
      "totalElapsed": 4200,
      "totalRemaining": 10200,
      "currentStage": 1,
      "totalStages": 3,
      "stageElapsed": 600,
      "stageRemaining": 7200,
      "stagePhase": "HOLD"
    }
  ],
  "uptime": 12345
}
```

| Field | Description |
|------|----------|
| `mode` | `"IDLE"`, `"DRYING"`, `"STORAGE"`, `"PROFILE"`, `"FAULT"` |
| `sessionNum` | Session number; absent in Idle/Fault |
| `target.temperature` | °C |
| `target.duration` | minutes (0 = infinite, for STORAGE) |
| `target.humidity` | % (only for STORAGE) |
| `totalElapsed` | seconds since the start of the mode |
| `totalRemaining` | seconds until the end of the program |
| `currentStage` | stage index (0-based, PROFILE only) |
| `totalStages` | total number of stages (PROFILE only) |
| `stageElapsed`, `stageRemaining` | seconds in the current stage (PROFILE) |
| `stagePhase` | `"RAMP"` or `"HOLD"` (PROFILE) |
| `uptime` | device uptime in seconds |

---

## `weights` (QoS 1, not retained)

```json
{
  "weights": [
    { "sensorId": "W1", "value": 823.4, "unitId": "U1" }
  ]
}
```

| Field | Type | Note |
|------|-----|------------|
| `sensorId` | string | `"W1"`...`"W4"` |
| `value` | float | grams, `weightGramsC10 / 10` |
| `unitId` | string | `"U1"`...`"U4"` - sensor-to-chamber binding |

Publication happens on a weight interval or on a weight change (usually once every 10 seconds).

---

## `rfid` (QoS 1, retained)

### tag_detected / tag_removed event

```json
{
  "unitId": "U1",
  "event": "tag_detected",
  "tag": "DEADBEEF12345678",
  "readerId": 0
}
```

| Field | Type | Note |
|------|-----|------------|
| `unitId` | string | `"U1"`...`"U4"` |
| `event` | string | `"tag_detected"` or `"tag_removed"` |
| `tag` | string | HEX tag ID (empty for `tag_removed`) |
| `readerId` | **number** | 0-3 (raw index, not `"R1"`) |

!!! warning "readerId is a number, not a string"
    Unlike `unitId`/`sensorId`, the `readerId` field in MQTT remains a **number** (0-3). If you parse on the backend, do not expect the string `"R1"`.

### Tag data (RFID read)

In response to `commands/read_rfid`, LINK assembles the `RfidReadData` fragments and publishes to the same `rfid` topic:

```json
{
  "readerId": 0,
  "unitId": "U1",
  "tagId": "DEADBEEF12345678",
  "format": "unknown",
  "data": "BASE64_STRING..."
}
```

| Field `format` | When | `data` |
|---------------|-------|--------|
| `"empty"` | All tag bytes are 0 - empty tag | `null` |
| `"openprinttag"` | The MIME signature `application/vnd.openprinttag` is found in the first 128 bytes (Prusa OpenPrintTag) | base64 string |
| `"openspool"` | The MIME signature `application/vnd.openspool` is found in the first 128 bytes | base64 string |
| `"unknown"` | None of the known signatures were found - parsed by the portal | base64 string |

Detection happens in `TelemetryPublisher::publishRfidData` by searching for the substring in the first 128 bytes of the buffer.

---

## `events` (QoS 1, not retained)

Application errors, warnings, diagnostic events.

### From UART `Log` (0x60)

```json
{
  "severity": "error",
  "source": "HEATER",
  "event": "OVER_MAX",
  "message": "Heater over max temperature",
  "unitId": "U1",
  "timestamp": "2026-04-19T12:00:00Z"
}
```

### From UART link errors (reference Link firmware)

```json
{
  "severity": "error",
  "source": "UART",
  "event": "PROTOCOL_ERROR_LOCAL",
  "message": "CRC mismatch on seq=42",
  "timestamp": "2026-04-19T12:00:00Z"
}
```

!!! warning "There is no automatic publication"
    `UartBridge` does **not** publish to `events` automatically when it receives a `Log` frame - it only calls `logHandler_`. Application code must call `MqttClient::publishEvent(doc)` itself. The reference Link firmware does this in `IdryerDevice::handleLog`. Without your own callback, the `events` topic will stay empty.

---

## `config` (QoS 1, not retained)

Full device menu JSON (up to about 3 KB). Published:

- in response to `commands/get_config`;
- after the config assembly from UART fragments completes.

Format:

```json
{
  "v": 8,
  "units": 3,
  "active": 0,
  "lang": "en",
  "menu": [
    { "id": 3, "t": "val", "val": [50, 65, 85] },
    { "id": 5, "t": "val", "val": [40, 15] },
    { "id": 17, "t": "val", "val": [55, 60, 55] },
    ...
  ]
}
```

| Field | Purpose |
|------|------------|
| `v` | Config schema version (revision) |
| `units` | Number of active chambers |
| `active` | Active chamber index (0-based) |
| `lang` | UI language code, `"en"` / `"ru"` |
| `menu[]` | Array of menu items with their current values |

The structure and possible values of menu items are defined in `menu_meta.h` and are agreed between the MCU and LINK at build time.

---

## `config/delta` (QoS 1, not retained)

Incremental menu update after `set` / `invoke`:

```json
{
  "d": {
    "3": [55, 60, 55],
    "81": 3
  }
}
```

The key is the menu item `id`, the value is the new content. The publisher sends this when the MCU changes something.

---

## `offline` (LWT, not for manual publication)

Published by the broker on an unexpected LINK disconnect:

```json
{}
```

The device itself never writes anything to this topic - EMQX always does it as part of the LWT mechanism.

---

## Common notes

- The `"YYYY-MM-DDTHH:MM:SSZ"` UTC `Timestamp` is added by `MqttClient` to all JSON publications where this is implemented (info, telemetry, status, events).
- JSON size is limited by the `StaticJsonDocument<N>` buffer in the publisher code. For `telemetry`, it is 1024 bytes (enough for 4 chambers).
- Empty arrays - `units` is never empty in `info` (at least 1 element if `unitsCount >= 1`).

---

## Next

- [04-backend-to-device.md](04-backend-to-device.md) - JSON commands from the cloud to the device.
- [05-examples.md](05-examples.md) - ready-to-use `mosquitto_sub` commands to inspect these JSON payloads on a live device.
