# Portal Developer Contract: LINK Integrations

One document. Everything a portal backend/frontend developer needs to implement passing settings for the three integrations (Home Assistant, Bambu Lab, Moonraker) into LINK and to show their status in the UI. The detailed reasoning and semantics are in separate documents, linked inline.

!!! note "Status"
    The LINK side is **not implemented**. The contract is fixed, so the portal can be developed in parallel. A LINK test bench will appear after implementation; until then, you can emulate responses manually with `mosquitto_pub`.

---

## Standard Function: MCU Menu + Remote Config

**All devices in the iDryer family** (dryers, heaters, telemetry - any `deviceType`) must have:

- **A local menu on the MCU screen** with settings items described in `menu_meta.h`.
- **Remote config**: the same menu items are available remotely through the standard MQTT commands - `commands/get_config`, `commands/set`, `commands/invoke`. Flow: `config` (retained, full snapshot) ↔ `config/delta` (changes).

Portal UI consequence: the **gear icon on the device card in the dashboard** opens a modal for editing all menu items of that device. **This feature is the same for iDryer, iHeater, and any future products** - it is not specific to LINK integrations.

Remote config protocol details: [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md).

---

## Architecture (in one paragraph)

The user clicks **"Home Assistant"**, **"Bambu Lab"**, or **"Moonraker"** in the portal UI → a modal with fields opens → the user fills it in → clicks **Save** → the backend **publishes an MQTT message** to the device (it does **not** store credentials) → LINK stores it in NVS → LINK publishes `integrations/status` → the UI shows the current state. The active integration is selected separately (in the MCU/LINK menu), and all three can exist in NVS at the same time.

## Manual chamber temperature control for iHeater - existing `commands/drying` channel

For `deviceType: heater` / `link_ii`, the **"Set chamber temperature" card on the dashboard** uses exactly the same `commands/drying` as iDryer - **no new command is needed**.

```json
// idryer/<serial>/commands/drying
{
  "unitId": "U1",
  "params": {
    "temperature": 50,
    "duration": 0
  }
}
```

- `temperature` - chamber target temperature in °C.
- `duration: 0` - indefinite.
- `duration: N` - automatic turn-off after N minutes.

The portal UI for `deviceType: heater` may show the label "Chamber Temperature" instead of "Drying Temperature", but the JSON body is identical.

### Three chamber temperature sources for iHeater

For the iHeater MCU there are three independent inputs - the protocol separates them, and firmware decides priority:

1. **`VIRTUAL_CHAMBER.target`** (Moonraker → LINK → UART) - automatic, tied to printing.
2. **`commands/drying`** (portal → MQTT → LINK → UART `CommandPayload`) - manual remote.
3. **The iHeater screen menu** - local fallback.

Recommended priority in firmware: Moonraker during active printing > drying > menu. This is NOT a protocol contract - the iHeater MCU firmware decides.

---

## ⚠️ Behavior depends on `deviceType`

The same connection (Bambu LAN MQTT or Moonraker WS) is used **differently** depending on the device type:

| `deviceType` (from `info`) | Mode | What LINK does with the Bambu connection | What LINK does with the Moonraker connection |
|--------------------------|------|-------------------------------------------|----------------------------------------------|
| `"dryer"` / `Unknown (legacy)` | **Writer** | Sends `ams_filament_setting` via `bambu_apply` (RFID tag emulation) | (MVP not used) |
| `"heater"` / `"link_ii"` | **Reader** | Subscribed to `device/<serial>/report`, reads filament/status → to MCU for chamber heating control | Subscribed to `gcode_macro VIRTUAL_CHAMBER.target` → sends the value to MCU (0 = off, >0 = chamber temperature) |

**What this means for the portal backend:**

- **Publish `bambu_apply` ONLY if** `info.deviceType in ["dryer", absent (legacy)]`.
- **For `heater` / `link_ii`** - never publish `bambu_apply`. They do not need it and ignore it.
- Settings (`link_integration`) are published **identically** for all types - the difference is only in `bambu_apply`.

---

## What the backend does

### 1. HTTP endpoint for the frontend (one for all three)

```
POST /devices/:deviceId/link-integration
Authorization: Bearer <user JWT>
Content-Type: application/json
```

The body is one of the three payloads (see the "Payload" section). The backend:

1. Verifies ownership: `deviceId` belongs to the user from the JWT.
2. Verifies `isOnline`: the Link is in `BOUND` state and online in MQTT.
3. Publishes to MQTT: `idryer/<serial>/commands/link_integration` with the same body, QoS 1, retain false.
4. **Does not store** `password`, `lanAccessCode`, or `apiKey` in the DB.
5. Returns `200 OK { "published": true }` or `409 Conflict` if the device is offline.

Recommendation: optionally store **non-secret metadata** (whether HA was configured; Bambu IP without lanAccessCode) for easier UI display - but only **masked and non-secret**.

### 2. Handling `integrations/status` from LINK

The backend subscribes (in the MQTT auth hook or in the main service) to `idryer/+/integrations/status`. When it receives a message:

- Unpack the JSON.
- Forward it to the frontend through WebSocket/SSE so the user sees the live change.

Optionally cache the last known snapshot for device list display - it is a retained topic, so even if the device is offline the frontend gets the last state.

### 3. Bambu-specific: publish `bambu_apply` on `tag_detected`

**Only for `deviceType == "dryer"` (or legacy/Unknown).** Do not publish for Heater/LinkII.

This is a separate flow described in [05-bambu-integration.md](05-bambu-integration.md). When the backend receives `idryer/<serial>/rfid` with `event: tag_detected`:

1. Check `deviceType` from the stored device `info`:
   - `"dryer"` or absent → continue.
   - `"heater"` / `"link_ii"` / anything else → **stop**, do not publish.
2. Find the spool by `uid`.
3. If found and the spool has a filament profile: build the Bambu-compatible payload (see below).
4. Publish `idryer/<serial>/commands/bambu_apply`.

Important: for `deviceType == "heater"`, `tag_detected` events will not arrive anyway - iHeater has no RFID readers (`info.units[].rfid` will be empty arrays). But for future safety, check `deviceType` explicitly instead of relying only on the event presence.

---

## What the frontend does

### Three buttons in the device card

- **Home Assistant** → modal with `host`, `port`, `username`, `password`, `discoveryPrefix`.
- **Bambu Lab** → modal with `ip`, `serial`, `lanAccessCode`, `defaultAmsId` (255), `defaultTrayId` (254), `autoApplyOnTagDetect` (true).
- **Moonraker** → modal with `host`, `port` (7125), `apiKey` (optional), `ssl` (false), `pollIntervalMs` (1000).

Buttons are **disabled + tooltip** if Link is not in `BOUND` or the device is offline.

### Saving

When the user clicks **Save** in any modal:

```
POST /devices/:deviceId/link-integration
{ "type": "ha"|"bambu"|"moonraker", "enabled": true, ... }
```

### Status display

Read `integrations/status` (through backend WebSocket/SSE). For each section (`ha`, `bambu`, `moonraker`) show:

- `configured` → indicator "configured / not configured".
- `state` → colored badge (`online` = green, `connecting` = yellow, `error`/`config_missing` = red).
- `lastError` → tooltip with the error text.
- Bambu specifics: `printerIp`, `printerSerial`, `printerState`, `progress`.
- Moonraker specifics: `host`, `printerState`, `progress`, `filename`.

### Active integration

The interface should show which of the three is **active now** (from `integrations/status.active`). Change it through the device menu (the `activeIntegration` field in the MCU menu). This is not a separate modal button; it is the general remote config mechanism - see [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md).

**Optional** in MVP: you can add a fast toggle in the portal UI that sends `commands/set { "id": <menu-id-active>, "val": "ha"|"bambu"|"moonraker"|"none" }` directly.

---

## Payload: `commands/link_integration`

Published to `idryer/<serial>/commands/link_integration`, QoS 1, retain false.

The discriminator is the `type` field. Partial payload: the portal publishes only one section at a time, and LINK merges it into NVS on top of the existing data.

### type = `"ha"`

```json
{
  "type": "ha",
  "enabled": true,
  "host": "homeassistant.local",
  "port": 1883,
  "username": "mqtt_user",
  "password": "secret",
  "discoveryPrefix": "homeassistant"
}
```

All fields except `type`, `enabled` are optional. Defaults are in [04-home-assistant.md](04-home-assistant.md).

### type = `"bambu"`

```json
{
  "type": "bambu",
  "enabled": true,
  "ip": "192.168.1.50",
  "serial": "039D09C40012345",
  "lanAccessCode": "12345678",
  "defaultAmsId": 255,
  "defaultTrayId": 254,
  "autoApplyOnTagDetect": true
}
```

### type = `"moonraker"`

```json
{
  "type": "moonraker",
  "enabled": true,
  "host": "klipper.local",
  "port": 7125,
  "apiKey": null,
  "ssl": false,
  "pollIntervalMs": 1000
}
```

---

## Payload: `commands/bambu_apply`

Published to `idryer/<serial>/commands/bambu_apply`, QoS 1, retain false. Separate flow - only for Bambu on `tag_detected`.

**Publish only for `deviceType == "dryer"` (or legacy/Unknown).** Do not send for Heater/LinkII.

```json
{
  "amsId": null,
  "trayId": null,
  "trayType": "PLA",
  "colorHex": "FFAABBFF",
  "nozzleTempMin": 209,
  "nozzleTempMax": 231,
  "trayInfoIdx": "GFL99",
  "settingId": "",
  "spoolId": "uuid-abc-123",
  "uid": "AABB1234"
}
```

### How the backend fills the fields

Sources (field names depend on the portal DB schema):

Field in payload | Source
---|---
`trayType` | `filamentType.code` (PLA / ABS / PETG / ...) |
`colorHex` | `filamentSpec.colorHex` or `materialSnapshot.colorHex`; 8 hex + FF or 6 hex |
`nozzleTempMin` | `printPreset.printNozzleTemp * 0.95` |
`nozzleTempMax` | `printPreset.printNozzleTemp * 1.05` |
`trayInfoIdx` | mapping `(filamentType, brand)` → Bambu code (`GFL99`, `GFA00`, ...); table in backend code |
`settingId` | `""` |
`amsId` / `trayId` | `null` - LINK inserts defaults from NVS |
`spoolId` | spool UUID (for diagnostics) |
`uid` | tag UID from `tag_detected` |

### `trayInfoIdx` table

Bambu code family (partial list, enough for MVP):

Type | Brand generic | Bambu Basic
---|---|---
PLA | `GFL99` | `GFA00`
PETG | `GFL98` | `GFG00`
ABS | `GFL97` | `GFB00`
TPU | `GFL96` | `GFU00`
ASA | `GFL95` | `GFB01`
PC | `GFL94` | `GFC00`
PA (nylon) | `GFL93` | `GFN03`

Full table is in the OpenSpool documentation and `OpenBambuAPI`.

---

## Topic: `integrations/status` (LINK → portal)

```
Topic:    idryer/<serial>/integrations/status
QoS:      1
Retained: true
```

A full snapshot, published on any change to any field in any of the three sections.

```json
{
  "active": "moonraker",
  "ha": {
    "configured": true,
    "enabled": true,
    "state": "disabled",
    "host": "homeassistant.local",
    "brokerPort": 1883,
    "authUsed": true,
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:00Z"
  },
  "bambu": {
    "configured": true,
    "enabled": false,
    "state": "disabled",
    "printerIp": "192.168.1.50",
    "printerSerial": "039D09C40012345",
    "lastError": "",
    "updatedAt": "2026-04-19T11:55:00Z"
  },
  "moonraker": {
    "configured": true,
    "enabled": true,
    "state": "online",
    "host": "klipper.local",
    "port": 7125,
    "printerState": "printing",
    "progress": 42,
    "remainingSeconds": 3600,
    "filename": "benchy.gcode",
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:12Z"
  }
}
```

### `state` values (common)

- `"disabled"` - `active != this type`, client disabled.
- `"idle"` - active, settings exist, waiting for work.
- `"connecting"` - connection attempt in progress.
- `"online"` - working.
- `"config_missing"` - active, but parameters are invalid.
- `"error"` - error, see `lastError`.

### Additional Bambu fields

Common fields (present in both modes): `printerIp`, `printerSerial`, `printerState`, `progress`, `remainingSeconds`, `currentLayer`, `totalLayers`, `nozzleTemp`, `nozzleTarget`, `bedTemp`, `bedTarget`.

Only in **Writer mode** (`deviceType: dryer`): `lastApply { at, result, spoolId, amsId, trayId }`.

Only in **Reader mode** (`deviceType: heater / link_ii`): `currentFilament` (the filament type of the current print, read from the printer), plus the chamber-control-specific fields - they are sent to the iHeater MCU, but can also be published in status for the UI.

### Additional Moonraker fields

- `host`, `port`, `virtualChamberAvailable`, `chamberHasSensor`, `chamberTarget`, `chamberTemperature`, `printerState`, `progress`, `remainingSeconds`, `filename`, `currentLayer`, `totalLayers`, `nozzleTemp`, `nozzleTarget`, `bedTemp`, `bedTarget`, `printDurationSeconds`.

Key fields for iHeater:

- **`chamberTarget`** (float, °C) - `VIRTUAL_CHAMBER.target` from Klipper (set through `M141 S<temp>` in the slicer). Heating setpoint.
- **`chamberTemperature`** (float, °C) - `VIRTUAL_CHAMBER.temperature`. PID feedback. Valid only when `chamberHasSensor == true`.
- **`chamberHasSensor`** (bool) - whether there is a real temperature sensor feeding the macro value. `false` → MCU iHeater uses its own local sensor.
- **`virtualChamberAvailable`** (bool) - Klipper returned the `gcode_macro VIRTUAL_CHAMBER` object. `false` → show the user the setup instructions (verified guide: `/docs/iHeater-link/virtual_chamber_guide.md`).

### Status publication frequency

- On any structural change (active, configured, connection state, target, hasSensor, lastApply) - **immediately**.
- Additionally **every 30 seconds** - so `chamberTemperature` and `progress` do not go stale in the retained snapshot. The timer resets on any explicit publication.
- No spam: not more often than once every 30 seconds between events.

### `printerState` values

Unified for both (Bambu and Moonraker):

- `"idle"` - idle
- `"prepare"` - Bambu preheat / Klipper preparing
- `"printing"` - printing
- `"paused"` - paused
- `"finished"` / `"complete"` - done
- `"cancelled"` - cancelled (Klipper only)
- `"error"` - print error

---

## Tag detected → bambu_apply: sequence (only `deviceType: dryer`)

```
RP2040 → UART Rfid(tag_detected, uid=AABB1234) → LINK
LINK → MQTT idryer/<serial>/rfid { "event": "tag_detected", "tag": "AABB1234", ... }
Backend:
  1. Check: info.deviceType == "dryer" or absent? No → stop.
  2. Find spool by uid.
  3. Build bambu_apply payload.
  4. Publish → idryer/<serial>/commands/bambu_apply
LINK (Dryer):
  - If active != bambu or !configured: ignore it, update status.state = "disabled" / "config_missing".
  - Otherwise: connect to the printer, send ams_filament_setting, update status.lastApply.
```

The backend publishes `bambu_apply` if `deviceType == "dryer"`. LINK (if the active integration is not Bambu) still decides whether to apply it or not - the double check does not hurt.

For iHeater (`deviceType: "heater"` / `"link_ii"`), this flow does not start: they have no RFID readers and the backend filters them by `deviceType`.

---

## Security

- The backend **never** stores `password` (HA), `lanAccessCode` (Bambu), or `apiKey` (Moonraker) in the DB.
- If you need to show "the field was filled", store only a `configured: true/false` marker, not the value. LINK returns the markers in `integrations/status`.
- Mask these fields in backend logs (`****`).
- Rate limit on `POST /devices/:id/link-integration`: 10 / 60 s (same as the existing `/provision` endpoints).

---

## Command-line check (before LINK is ready)

Emulate LINK manually:

```bash
# Subscribe to commands (play the role of LINK)
mosquitto_sub -h BROKER -p 8883 --cafile ca.pem \
  -u $SERIAL -P $TOKEN \
  -t "idryer/$SERIAL/commands/link_integration" \
  -t "idryer/$SERIAL/commands/bambu_apply" -v

# Publish a status reply (play the role of LINK)
mosquitto_pub -h BROKER -p 8883 --cafile ca.pem \
  -u $SERIAL -P $TOKEN -q 1 -r \
  -t "idryer/$SERIAL/integrations/status" \
  -m '{ "active":"none", "ha":{...}, "bambu":{...}, "moonraker":{...} }'
```

The frontend sees status and shows the state. You can debug the whole UI before LINK is ready.

---

## Portal developer checklist

Backend:

- [ ] `POST /devices/:deviceId/link-integration` - endpoint with ownership + online checks. Same for all `deviceType`s.
- [ ] Publish to MQTT `idryer/<serial>/commands/link_integration` without storing secrets in the DB.
- [ ] Subscribe to `idryer/+/integrations/status`, route it to the frontend.
- [ ] (Bambu) When receiving `idryer/<serial>/rfid` with `tag_detected`:
  - [ ] Check `info.deviceType`. If `heater` / `link_ii` / any non-dryer - **stop**.
  - [ ] Otherwise: find spool, publish `commands/bambu_apply`.
- [ ] (Bambu) Implement mapping `(type, brand) → trayInfoIdx` (Bambu code table).
- [ ] Mask passwords/codes/keys in logs.
- [ ] Rate limit the new endpoint.

Frontend:

- [ ] Three buttons in DeviceShow: HA / Bambu / Moonraker, disabled if the device is offline.
- [ ] Three modal dialogs with fields (see the "Payload" section).
- [ ] POST on Save, handle 200/409.
- [ ] Visualize `integrations/status` (badge by `state`, tooltip with `lastError`).
- [ ] (Optional) Quick active-integration switch through `commands/set`.

Test scenario:

- [ ] Open the HA modal → Save → see `state: "disabled"` (while not active) and `configured: true`.
- [ ] Select HA in the device menu → `state: "online"` → sensors appear in HA.
- [ ] Do the same for Bambu, Moonraker.
- [ ] Send a fake `tag_detected` → see `bambu_apply` in MQTT and `lastApply: "ok"` in status.

---

## Links

- Common contract and principles: [03-link-integrations-overview.md](03-link-integrations-overview.md).
- HA specifics: [04-home-assistant.md](04-home-assistant.md).
- Bambu specifics: [05-bambu-integration.md](05-bambu-integration.md).
- Moonraker specifics: [06-moonraker-printer.md](06-moonraker-printer.md).
- Archived Bambu design plan (original statement): `../../../BAMBU_EMULATION_PLAN.md` in the root of the iDryerRP2040 repository.
