# Moonraker (Klipper): Reading Printer Status over WebSocket

LINK connects to the user's Moonraker host (a Raspberry Pi with Klipper) over WebSocket and reads print status: state, progress, current layer, temperatures. It is always **Reader mode** only (unlike Bambu, which also has Writer mode for Dryer).

## Target devices and purpose

| `deviceType` | Purpose | MVP |
|--------------|---------|-----|
| `Heater` / `LinkII` | **Main scenario.** iHeater reads the **chamber target temperature from `VIRTUAL_CHAMBER.target`** → the MCU heats the chamber to that temperature. | ✅ |
| `Dryer` | Read print status for the UI ("PLA is printing") and optionally synchronize the drying start. | optional, later |

The `commands/link_integration` contract with `type: "moonraker"` is the same for all `deviceType`s. LINK decides what to do with the status: pass it to the MCU (iHeater) or publish it for display (iDryer).

The `bambu_apply` command does **not exist** in the Moonraker context - in MVP, LINK does not write anything to Klipper.

## How iHeater gets the chamber temperature - VIRTUAL_CHAMBER macro

!!! note "Verified in `iHeater-link`"
    This scheme already works on a live setup. Full user guide: [/docs/iHeater-link/virtual_chamber_guide.md](../../../../iHeater-link/virtual_chamber_guide.md).

### The idea in two sentences

The user adds a special `[gcode_macro VIRTUAL_CHAMBER]` with a `target` variable on the Klipper host. In the slicer's start G-code, they write `M141 S50` - Klipper writes `50` into `VIRTUAL_CHAMBER.target` through the macro wrapper. LINK subscribes to that value and immediately tells the MCU: "the chamber is 50°C".

### What the user must do

Minimal variant (without a chamber temperature sensor):

```ini
[gcode_macro VIRTUAL_CHAMBER]
variable_target: 0
variable_temperature: 0
variable_has_sensor: 0
gcode:

[gcode_macro M141]
gcode:
  {% set t = params.S|default(0)|float %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE={t}

[gcode_macro M191]
gcode:
  {% set t = params.S|default(0)|float %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=target VALUE={t}
```

With a sensor (recommended), add `delayed_gcode` that refreshes `variable_temperature` every 2 seconds from the real `[temperature_sensor chamber]` in `printer.cfg`:

```ini
[gcode_macro VIRTUAL_CHAMBER]
variable_target: 0
variable_temperature: 0
variable_has_sensor: 1        ; 1 = the sensor exists, LINK may use temperature
gcode:

[delayed_gcode UPDATE_VIRTUAL_CHAMBER_TEMP]
initial_duration: 2
gcode:
  {% set t = printer["temperature_sensor chamber"].temperature|default(0) %}
  SET_GCODE_VARIABLE MACRO=VIRTUAL_CHAMBER VARIABLE=temperature VALUE={t}
  UPDATE_DELAYED_GCODE ID=UPDATE_VIRTUAL_CHAMBER_TEMP DURATION=2
```

Replace `temperature_sensor chamber` with the name of your sensor object in Klipper.

Why `M141`/`M191`:

- `M141 S<temp>` - the standard G-code command to set chamber target temperature.
- `M191 S<temp>` - "set target and wait until it is reached".
- Most slicers (Orca, SuperSlicer, Cura) can insert them into the start G-code.
- The user **does not change the slicer** - they only enable this command in the material profile template.

Why `temperature` and `has_sensor`:

- `target` - where to heat to (setpoint). Set by `M141`/`M191`.
- `temperature` - the current chamber temperature (feedback) for PID on the iHeater MCU side.
- `has_sensor` - indicates that `temperature` is valid. `0` means LINK does not treat the value as reliable, and the MCU uses its own sensor.

### What LINK iHeater does

1. When `active == "moonraker"`, subscribes to three fields of `gcode_macro VIRTUAL_CHAMBER`: `target`, `temperature`, `has_sensor`.
2. Receives the initial snapshot and stores all three values.
3. Listens for `notify_status_update` - when any field changes, it calls `VirtualChamberCallback` with the full `VirtualChamberData` structure.
4. Sends `target`, `temperature`, `hasSensor` to the MCU over UART.
5. iHeater MCU:
   - `target > 0` + `hasSensor == true` → PID closes on the temperature from Klipper.
   - `target > 0` + `hasSensor == false` → MCU uses its own local sensor (fallback).
   - `target == 0` → heating turns off.

The "material → temperature" table is **not needed**. The printer (through the user/slicer) assigns the target value itself.

### Verified exchange (JSON-RPC)

Request (initial snapshot):

```json
{
  "jsonrpc": "2.0",
  "method": "printer.objects.query",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target", "temperature", "has_sensor"]
    }
  },
  "id": 1
}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": {
      "gcode_macro VIRTUAL_CHAMBER": {
        "target": 50.0,
        "temperature": 27.85,
        "has_sensor": 1
      }
    },
    "eventtime": 3201823.125032914
  },
  "id": 1
}
```

Subscription instead of polling:

```json
{
  "jsonrpc": "2.0",
  "method": "printer.objects.subscribe",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target", "temperature", "has_sensor"]
    }
  },
  "id": 1
}
```

Moonraker responds with the snapshot and then notifications when any of the three fields change:

```json
{
  "jsonrpc": "2.0",
  "method": "notify_status_update",
  "params": [
    { "gcode_macro VIRTUAL_CHAMBER": { "temperature": 28.12 } },
    <eventtime>
  ]
}
```

The notification contains only changed fields. LINK stores the last full snapshot and merges the changes.

### If the user does not have VIRTUAL_CHAMBER

LINK still subscribes (Moonraker will not error - the `gcode_macro VIRTUAL_CHAMBER` object simply will not exist and the response will be empty). In this case, `target` is treated as `0` - the chamber does not heat. `integrations/status.moonraker.lastError` contains a hint: `"VIRTUAL_CHAMBER macro not configured in Klipper"`.

The portal UI can show the user: "To enable automatic chamber control, install `virtual_chamber.cfg` in Klipper - instruction".

### Manual mode

Without `VIRTUAL_CHAMBER`, the user can still control the chamber manually from the iHeater menu (set the temperature locally). In this case, the Moonraker integration only provides print status for the UI (see below), but does not control heating.

!!! note "Status"
    **Fully design-level.** Nothing described here is implemented in the LINK code. The contract between the portal and LINK is ready and agreed. The contract between LINK and Moonraker is the standard Moonraker API and is well known.

!!! warning "You are expected to know Klipper/Moonraker"
    This document uses Klipper ecosystem terms: `printer.cfg`, `gcode_macro`, `variable_*`, `M141/M191` G-code commands, `[temperature_sensor chamber]`, `delayed_gcode`, `printer.objects.subscribe`, JSON-RPC 2.0. If you have not worked with Klipper, first read the [Klipper docs](https://www.klipper3d.org/) (at least the Config + G-Code sections) and the [Moonraker API](https://moonraker.readthedocs.io/en/latest/web_api/). WebSocket and JSON-RPC are covered in the [glossary](../01-overview/04-glossary.md#websocket).

---

## Architecture

```
   iDryer LINK                    Klipper host (Raspberry Pi)
  ┌────────────────┐             ┌──────────────────────────┐
  │ MoonrakerClient├──WS ws://──►│ Moonraker (port 7125)    │
  │                │             │   JSON-RPC over WS       │
  │ Status parser  │             │                           │
  │                │             │     ┌──────────┐          │
  │ → integrations │             │     │ Klipper  │          │
  │   /status      │             │     │  klippy  │          │
  └────────────────┘             │     └──────────┘          │
                                  └──────────────────────────┘
```

LINK is a **client** of the Moonraker instance already running on the user's host. LINK does not install anything on the Klipper host.

---

## Configuration

Through the shared `commands/link_integration` ([03-link-integrations-overview.md](03-link-integrations-overview.md)):

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

Parameter | Required | Notes
---|---|---
`host` | yes | mDNS or IP of the Klipper host |
`port` | no, default `7125` | Moonraker standard |
`apiKey` | no | `null` = open; required if `moonraker.conf` has `[authorization]` with `force_logins: true` |
`ssl` | no, default `false` | `true` → `wss://`, otherwise `ws://` |
`pollIntervalMs` | no, default `1000` | subscribe or polling interval |

---

## Connection

1. LINK builds the URL: `ws://<host>:<port>/websocket` (or `wss://` if `ssl: true`).
2. Optionally adds `?token=<apiKey>` to the URL when a key is present.
3. Opens WebSocket and uses ping/pong keep-alive.
4. Reconnects with backoff after a disconnect.

Moonraker uses [JSON-RPC 2.0](https://www.jsonrpc.org/specification) over WebSocket.

---

## Subscribing to objects

After connecting, LINK subscribes to a set of objects. For iHeater, `gcode_macro VIRTUAL_CHAMBER` is **required** (main one), the rest are for the UI.

```json
{
  "jsonrpc": "2.0",
  "method": "printer.objects.subscribe",
  "params": {
    "objects": {
      "gcode_macro VIRTUAL_CHAMBER": ["target"],
      "print_stats": null,
      "virtual_sdcard": null,
      "extruder": ["temperature", "target"],
      "heater_bed": ["temperature", "target"],
      "display_status": null,
      "idle_timeout": null
    }
  },
  "id": 1
}
```

Moonraker returns the initial snapshot and then sends notifications when any field changes.

### Notification format

```json
{
  "jsonrpc": "2.0",
  "method": "notify_status_update",
  "params": [
    {
      "print_stats": { "state": "printing", "filename": "benchy.gcode", "print_duration": 120.5 },
      "display_status": { "progress": 0.42 },
      "extruder": { "temperature": 220.5, "target": 220 }
    },
    <eventtime>
  ]
}
```

LINK parses `params[0]` into a status object.

---

## Mapping into `integrations/status`

The `moonraker` section in the shared status:

```json
"moonraker": {
  "configured": true,
  "enabled": true,
  "state": "online",
  "host": "klipper.local",
  "port": 7125,
  "virtualChamberAvailable": true,
  "chamberHasSensor": true,
  "chamberTarget": 50.0,
  "chamberTemperature": 27.85,
  "printerState": "printing",
  "progress": 42,
  "remainingSeconds": 3600,
  "filename": "benchy.gcode",
  "currentLayer": null,
  "totalLayers": null,
  "nozzleTemp": 220.5,
  "nozzleTarget": 220,
  "bedTemp": 60.2,
  "bedTarget": 60,
  "printDurationSeconds": 1200,
  "lastError": "",
  "updatedAt": "2026-04-19T12:00:12Z"
}
```

**VIRTUAL_CHAMBER fields:**

| Field | Type | Description |
|-------|------|-------------|
| `virtualChamberAvailable` | bool | `true` - Moonraker returned the `gcode_macro VIRTUAL_CHAMBER` object; `false` - the user needs to add the macro in Klipper |
| `chamberHasSensor` | bool | `variable_has_sensor` in the macro: `true` → `chamberTemperature` is valid (feedback for PID) |
| `chamberTarget` | float, °C | `variable_target` - chamber target temperature; `0` = off |
| `chamberTemperature` | float, °C | `variable_temperature` - current temperature; valid only when `chamberHasSensor == true` |

### Publication frequency

To keep the retained snapshot fresh, especially `chamberTemperature`, LINK republishes `integrations/status` **every 30 seconds** even if nothing changes. On any structural change (target / hasSensor / available / other fields) the snapshot is sent immediately and the 30-second timer resets.

### Moonraker → status mapping

Moonraker | status field | Notes
---|---|---
`print_stats.state` | `printerState` | `"standby"`, `"printing"`, `"paused"`, `"complete"`, `"cancelled"`, `"error"` |
`display_status.progress` | `progress` | Moonraker gives 0-1, LINK publishes 0-100 |
`print_stats.filename` | `filename` |  |
`print_stats.print_duration` | `printDurationSeconds` |  |
`virtual_sdcard.progress` | alternative to `progress` if display_status is empty |
`extruder.temperature` | `nozzleTemp` |  |
`extruder.target` | `nozzleTarget` |  |
`heater_bed.temperature` | `bedTemp` |  |
`heater_bed.target` | `bedTarget` |  |

### `remainingSeconds`

Moonraker does not expose "remaining" directly - you need to compute it: `print_duration / progress * (1 - progress)`. Or take it from the gcode metadata (`print.stats.estimated_time`). In MVP, compute it from progress, `null` if progress = 0.

### `currentLayer` / `totalLayers`

By default Klipper does not know the layer count - it depends on the slicer. If the gcode contains `SET_PRINT_STATS_INFO TOTAL_LAYER=N CURRENT_LAYER=i` (modern slicers write it), Moonraker puts it into `print_stats.info`:

```json
"print_stats": {
  "info": { "total_layer": 285, "current_layer": 120 }
}
```

LINK copies that into `currentLayer` / `totalLayers`. If missing, use `null`.

---

## Errors and status codes

| Error | `state` | `lastError` |
|-------|---------|-------------|
| No WiFi | `connecting` | `"no wifi"` |
| Host does not resolve | `error` | `"host not resolved"` |
| TCP refused | `error` | `"connection refused (port 7125?)"` |
| WS handshake failed | `error` | `"ws handshake failed"` |
| `401 Unauthorized` | `error` | `"auth failed (check apiKey)"` |
| JSON-RPC method error | `online` + lastError | `"method error: <msg>"` |
| WS disconnect | `connecting` | `"reconnecting"` |

---

## LINK → Klipper commands (optional, not MVP)

Moonraker supports `printer.gcode.script`, `printer.print.start/pause/resume/cancel`. In MVP, LINK **does not send** anything, it only reads. If needed later, we can add a separate MQTT command `commands/printer_command`.

---

## Using the status

### iHeater (`deviceType: heater / link_ii`) - the main consumer

LINK iHeater sends the main field to the MCU over UART - **`chamberTarget` from `VIRTUAL_CHAMBER.target`**.

It can also pass through `printerState`, `progress`, `nozzleTarget`, and `bedTarget` for display on the iHeater screen. This is optional - chamber heating only needs `chamberTarget`.

iHeater MCU:

- `chamberTarget > 0` → turn on the chamber heater with target = `chamberTarget`.
- `chamberTarget == 0` → turn it off.

**The "material → temperature" table is not needed.** The user sets the chamber target temperature in the slicer (through `M141` / `M191` in the start G-code), and the printer passes it through `VIRTUAL_CHAMBER.target`.

The portal is not involved in this cycle. In parallel, LINK publishes `integrations/status` - for the app UI (displaying `chamberTarget` and print progress).

### Three independent inputs for the iHeater chamber target

iHeater MCU can receive the chamber target from three sources at the same time - the protocol separates them, and **the MCU firmware decides priority**:

1. **`VIRTUAL_CHAMBER.target`** (Moonraker → LINK → UART) - automatic during printing.
2. **`commands/drying`** from the portal (standard MQTT command, the same `CommandPayload` as for iDryer; `params.temperature` = chamber target, `params.duration = 0` = indefinite). See [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md).
3. **The local iHeater menu**.

Recommended priority in iHeater firmware: automatic (`VIRTUAL_CHAMBER`) during active printing overrides manual values; between `drying` and the menu - last write wins. This is **firmware policy**, not protocol contract.

### iDryer (`deviceType: dryer`) - status for the UI

In MVP, LINK iDryer only publishes `integrations/status` with Moonraker data. The frontend can:

- Show "PLA is printing, chamber U1 is drying that same PLA" in the UI.
- Automatically start chamber drying with the filament that matches `filename` (through manual mapping or heuristics) if that logic is implemented in backend/frontend.
- Stop drying when `printerState = "complete"`.

This logic lives in the portal backend/frontend, not in LINK. LINK just distributes status.

---

## MVP limitations

- One Moonraker host per one LINK.
- Read-only, no control.
- `currentLayer` / `totalLayers` only if the slicer writes the corresponding metadata into the gcode.
- Klipper without Moonraker (direct Klippy API) is not supported.

---

## What's Next

- [03-link-integrations-overview.md](03-link-integrations-overview.md) - common contract.
- [05-bambu-integration.md](05-bambu-integration.md) - alternative for Bambu owners.
- [07-portal-integration-contract.md](07-portal-integration-contract.md) - condensed reference for portal developers.
