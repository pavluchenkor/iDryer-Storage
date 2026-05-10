# Bambu Lab: LAN MQTT Integration

LINK connects to a Bambu Lab printer on the local network through LAN MQTT. The goal is to automatically apply filament settings in AMS/Tray when an RFID tag is detected, and to display printer status in the iDryer app.

The architectural decision (from `BAMBU_EMULATION_PLAN.md`) is: **do not emulate the native Bambu RFID in hardware**, but send `ams_filament_setting` to the printer over LAN MQTT, as OpenSpool does. The Bambu integration on LINK is limited to the network path.

## Two modes, selected by `deviceType`

The same connection (LAN MQTT) is used differently depending on the device:

| `deviceType` (from `info`) | Mode | What LINK does |
|--------------------------|------|-----------------|
| `Dryer` (0x01) or legacy | **Writer** - RFID tag emulation | Writes `ams_filament_setting` to the printer on `bambu_apply` from the portal |
| `Heater` (0x02) / `LinkII` (0x05) | **Reader** - status readout | Subscribed to `device/<serial>/report`, reads filament type and print status, sends it to the MCU for chamber heating control |

The credentials contract (`commands/link_integration`) is the same. The difference is the consumer logic after the connection is established. Details for each mode are below.

!!! note "Status"
    - The **portal (backend + frontend)** is implemented: `POST /devices/:id/configure-bambu`, UI dialog in DeviceShow.
    - **LINK** is **not implemented**: there is no `BambuClient`, no handlers for `commands/bambu_config` or `commands/bambu_apply`, and no `integrations/status` / `bambu/status` publication.
    - This document is a design-level contract between the portal and LINK.

!!! warning "You are expected to know the Bambu Lab ecosystem"
    This document uses Bambu terms: AMS (Auto Material System, the multi-filament module), Tray (a spool slot in AMS), LAN Mode / LAN Access Code (local access mode and its 8-digit code), `ams_filament_setting` (the internal Bambu MQTT command), and OpenSpool (an open RFID spool standard). If you have not worked with a Bambu X1C/P1S, first read the [OpenSpool docs](https://openspool.io/) and [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI).

---

## Writer mode (iDryer): end-to-end

Applies when `deviceType == "dryer"` (or legacy/Unknown).

```
1. The user brings a spool to the iDryer reader.
2. RP2040: PN532 detects the tag → Rfid (0x14) → LINK.
3. LINK publishes idryer/<serial>/rfid { "event": "tag_detected", "tag": "AABB1234", ... }.
4. Backend: checks deviceType == "dryer" → finds a spool by UID → builds a Bambu payload (OpenSpool-compatible).
5. Backend publishes idryer/<serial>/commands/bambu_apply.
6. LINK (if active == "bambu" + configured): connects to the Bambu printer over MQTT and sends ams_filament_setting.
7. LINK publishes idryer/<serial>/integrations/status with the apply result.
8. The printer updates the filament parameters.
```

## Reader mode (iHeater): end-to-end

Applies when `deviceType in ["heater", "link_ii"]`.

```
1. The user starts a print on the Bambu printer (through the Bambu app, directly, or any other way).
2. The Bambu printer publishes to device/<serial>/report: gcode_state=RUNNING, tray_type=ABS, tray_info_idx=..., tray_temper=240, ...
3. LINK iHeater (subscribed to that topic): parses the report and extracts the current filament and state.
4. LINK → UART → iHeater MCU: "ABS print is running, tray_temper 240°C".
5. iHeater MCU: applies its "ABS → chamber 45°C" table and turns on the chamber heater.
6. LINK publishes idryer/<serial>/integrations/status in parallel with printerState/currentFilament for the UI.
7. The printer finishes (`gcode_state=FINISH`) → LINK → MCU → MCU turns heating off.

The backend does not publish `commands/bambu_apply` in this case (`deviceType != dryer` → filtered out on the backend).
```

---

## Configuration

Settings arrive in LINK through the shared `commands/link_integration` channel. Details are in [03-link-integrations-overview.md](03-link-integrations-overview.md).

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

Parameter | Required | Notes
---|---|---
`ip` | yes | Printer IP (LAN). LINK and printer must be on the same network |
`serial` | yes | printer serial number |
`lanAccessCode` | yes | 8-digit code from the printer menu |
`defaultAmsId` | no, default `255` | default AMS (`255` = current active)
`defaultTrayId` | no, default `254` | default tray (`254` = without AMS, external spool)
`autoApplyOnTagDetect` | no, default `true` | if `false`, LINK ignores `bambu_apply` and does not auto-apply |

### LAN Mode on the printer

To use the integration, the user must:

1. On the printer: Settings → **LAN Only Mode** → note the code.
2. Enable "LAN Mode Liveview".

In newer Bambu firmware, part of the API may require Developer Mode - on the X1C this is a separate setting. If Developer Mode is off, `ams_filament_setting` still works, but other commands may not.

---

## Connecting to the Bambu printer

### Transport

- **MQTT TLS** (mandatory, not plain).
- Host: `ip` from the config.
- Port: **8883**.
- Client ID: any string, for example `idryer_<serial>_bambu`.
- Username: `bblp`.
- Password: `lanAccessCode`.
- CA: Bambu uses a **self-signed** certificate. In LINK code `WiFiClientSecure::setInsecure()` is used for LAN mode - this is the only working path, confirmed by OpenSpool.

### Printer topics

- **Subscribe** (LINK reads): `device/<printerSerial>/report` - periodic status reports.
- **Publish** (LINK writes): `device/<printerSerial>/request` - commands to the printer.

### Reconnect

LINK reconnects after a disconnect. Exponential backoff starts at 1 s, capped at 60 s.

---

## Command 1: `commands/bambu_apply` - only for Writer mode (Dryer)

Apply filament settings to the printer. Sent only to Dryer devices. The portal backend filters by `deviceType`.

### Topic

```
idryer/<serial>/commands/bambu_apply
QoS: 1
```

### Payload (from backend)

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

Field | Required | Notes
---|---|---
`amsId` | no | `null` → LINK uses `defaultAmsId` from NVS |
`trayId` | no | `null` → LINK uses `defaultTrayId` from NVS |
`trayType` | yes | `"PLA"`, `"PETG"`, `"ABS"`, `"TPU"`, `"ASA"`, `"PC"`, `"PA"`, ... |
`colorHex` | yes | 8 hex RGBA characters (`FFAABBFF`), or 6 - LINK adds `FF` |
`nozzleTempMin` | yes | integer, °C |
`nozzleTempMax` | yes | integer, °C |
`trayInfoIdx` | yes | 5-character Bambu material code (for example `"GFL99"` = PLA Basic Generic) |
`settingId` | no, default `""` | preset identifier, usually an empty string |
`spoolId` | no | spool UUID in the portal DB - for diagnostics and status |
`uid` | no | tag UID - for diagnostics |

Backend field-filling logic (from OpenSpool):

- `trayType` = `filamentType.code`.
- `colorHex` = `filamentSpec.colorHex || materialSnapshot.colorHex`.
- `nozzleTempMin/Max` = ±5% of `printPreset.printNozzleTemp`.
- `trayInfoIdx` = `getBambuCode(type, brand)` - table in the backend code.
- `settingId` = `""`.

### LINK behavior on receipt

1. Check: `active == "bambu"` and the bambu section in NVS is `configured`.
2. If `active != "bambu"` or `!configured`: publish `integrations/status` with `state: "config_missing"` or `state: "disabled"` and **ignore** the command.
3. Check `autoApplyOnTagDetect`: if `false`, ignore it (apply can be called only manually).
4. Resolve `amsId` / `trayId` (from payload or from NVS).
5. Build the Bambu MQTT payload `ams_filament_setting`:
   ```json
   {
     "print": {
       "sequence_id": "1",
       "command": "ams_filament_setting",
       "ams_id": <amsId>,
       "tray_id": <trayId>,
       "tray_info_idx": "<trayInfoIdx>",
       "tray_color": "<colorHex>",
       "nozzle_temp_min": <nozzleTempMin>,
       "nozzle_temp_max": <nozzleTempMax>,
       "tray_type": "<trayType>",
       "setting_id": "<settingId>"
     }
   }
   ```
6. Publish it to `device/<printerSerial>/request`.
7. Publish `integrations/status` with `state: "apply_ok"` or `"apply_failed"` plus `lastApply`.

The backend **always** sends `bambu_apply` when spool resolution succeeds - LINK decides whether to apply it. This allows `autoApplyOnTagDetect` to change without coordinating with the portal.

---

## Command 2: `commands/bambu_test` (optional)

Manual apply test without a tag event. Useful for a "Test apply" button in the portal UI.

```json
// payload is the same as for bambu_apply
```

LINK implementation: ignores `autoApplyOnTagDetect`, always tries to apply if `configured` and `active == "bambu"`.

---

## Reader mode (iHeater): reading printer status

For `deviceType in ["heater", "link_ii"]` - the **main** operating mode. LINK never writes to the printer (it does not have `bambu_apply` as a source), it only subscribes and reads.

The task is to determine **which filament is currently being printed** and pass that information to the MCU so the iHeater MCU can decide chamber temperature (the "material → chamber temperature" table lives in MCU firmware).

### What LINK reads from the report

Bambu sends `device/<serial>/report` at about 1 Hz (or on changes). LINK parses:

- `gcode_state` - print state (`IDLE` / `PREPARE` / `RUNNING` / `PAUSE` / `FINISH` / `FAILED`).
- `ams.tray[N].tray_type` - filament type of the current tray (when `RUNNING`).
- `ams.tray[N].tray_info_idx` - Bambu material code.
- `ams.tray[N].tray_color` - color in hex.
- `ams.tray[N].nozzle_temp_min/max` - nozzle temperature.
- `bed_temper` / `bed_target_temper` - bed temperature.
- `nozzle_temper` / `nozzle_target_temper` - nozzle temperature.
- `mc_percent`, `mc_remaining_time`, `layer_num`, `total_layer_num` - progress.

### What LINK sends to the iHeater MCU

Over UART. The exact format is the iHeater-LINK firmware task, but it must contain at least:

- `printerState`: enum (idle/printing/paused/finished/error)
- `currentFilament.type`: string (PLA/PETG/ABS/...)
- `currentFilament.nozzleTempMax`: temperature
- (Optional) `progress`, `bedTemp`, `nozzleTemp` - if the MCU wants to show them on the screen.

### How the iHeater MCU reacts

This is **iHeater firmware business logic**, not library logic:

- When `printerState == "printing"` → choose chamber temperature from the material-to-temperature table.
- Turn the chamber heater on with that target temperature.
- When `printerState == "finished" / "error" / "idle"` → turn the heater off.

In this mode, LINK is only the **status transport**. The heating logic lives on the MCU.

### The user can override manually

For `deviceType: heater` / `link_ii`, **two other chamber target sources** remain available (see also [06-moonraker-printer.md](06-moonraker-printer.md) - there the same three sources also work for Bambu):

- **`commands/drying`** from the portal (the card on the dashboard; the same JSON as for iDryer).
- **The local iHeater menu**.

Priority between the automatic source (Bambu `tray_type` + MCU table) and manual sources is the iHeater firmware policy.

## Common: publishing status to the iDryer cloud

Regardless of mode, LINK publishes the `bambu` section in the shared `integrations/status`:

```json
"bambu": {
  "configured": true,
  "enabled": true,
  "state": "online",
  "printerIp": "192.168.1.50",
  "printerSerial": "039D09C40012345",
  "printerState": "RUNNING",
  "progress": 42,
  "remainingSeconds": 3600,
  "currentLayer": 120,
  "totalLayers": 285,
  "nozzleTemp": 220.5,
  "nozzleTarget": 220,
  "bedTemp": 60.2,
  "bedTarget": 60,
  "lastApply": {
    "at": "2026-04-19T12:00:00Z",
    "result": "ok",
    "spoolId": "uuid-abc-123",
    "amsId": 0,
    "trayId": 1
  },
  "lastError": "",
  "updatedAt": "2026-04-19T12:00:12Z"
}
```

### Mapping Bambu report fields to status

Bambu `gcode_state` → `printerState`:

- `"IDLE"` → `"idle"`
- `"PREPARE"` → `"prepare"`
- `"RUNNING"` → `"printing"`
- `"PAUSE"` → `"paused"`
- `"FINISH"` → `"finished"`
- `"FAILED"` → `"error"`

Bambu `mc_percent` → `progress` (0-100).
Bambu `mc_remaining_time` → `remainingSeconds` (already in seconds, may arrive in minutes - verify at runtime).
Bambu `layer_num`, `total_layer_num` → `currentLayer`, `totalLayers`.
Bambu `nozzle_temper`, `nozzle_target_temper`, `bed_temper`, `bed_target_temper` → the corresponding fields.

### Request a full snapshot

Immediately after connecting, LINK sends:

```json
{ "pushing": { "sequence_id": "1", "command": "pushall" } }
```

to `device/<printerSerial>/request` - the printer responds with the full report.

---

## Error handling

| Error | LINK action | `state` | `lastError` |
|-------|-------------|---------|-------------|
| No WiFi | Reconnect when available | `connecting` | `"no wifi"` |
| TLS handshake failed | Retry with backoff | `error` | `"tls handshake failed"` |
| Auth rejected | Do not reconnect automatically | `error` | `"auth rejected (check lan access code)"` |
| Printer does not answer `request` | retry, track `pushing.pushall` | `online` + `lastError` | `"no response to request"` |
| Apply failed | Publish `apply_failed` in status | `online` + `lastError` | `"apply failed: ..."` |

---

## MVP limitations

- One Bambu printer per one LINK.
- Only LAN MQTT, not cloud.
- Only the Writer mode is required for the portal-side `bambu_apply` flow; Reader mode is for iHeater status use.
- The full Bambu API surface is not covered here.

---

## What's Next

- [03-link-integrations-overview.md](03-link-integrations-overview.md) - common contract.
- [04-home-assistant.md](04-home-assistant.md) - the next integration.
- [06-moonraker-printer.md](06-moonraker-printer.md) - Moonraker/Klipper.
