# LINK Integrations: Common Contract

LINK can work with external services in addition to the main iDryer cloud:

- **Home Assistant** - publish the device sensors to the user's own HA server.
- **Bambu Lab** - apply filament settings to a Bambu printer over LAN MQTT and read printer status.
- **Moonraker (Klipper)** - read printer status over WebSocket.

All three are built on the same architecture model. This page describes the **common contract**. The details of each integration are in separate documents:

- [04-home-assistant.md](04-home-assistant.md)
- [05-bambu-integration.md](05-bambu-integration.md)
- [06-moonraker-printer.md](06-moonraker-printer.md)

!!! warning "Status"
    **Design-level documentation.** On the `idryer-protocol` library side, neither `commands/link_integration` nor handlers for these three integrations are **implemented**. The portal has partial Bambu support (`/configure-bambu` endpoint). These documents fix the target contract before implementation starts.

---

!!! note "Menu and remote config are standard for all devices"
    Any device in the iDryer family (dryer, heater, telemetry module) has a local menu on the MCU screen and supports remote configuration of those items through the standard `commands/get_config` / `commands/set` / `commands/invoke`. The gear icon on the device card in the portal is a standard UI feature and is the same for all of them. This document describes **only** the contract for external LINK integrations (HA/Bambu/Moonraker), which live **separately** from the MCU menu. Do not confuse them.

---

## Principles

### 1. Credentials live only in LINK

The portal **does not store** `password`, `lanAccessCode`, or `apiKey`. The user enters them in the portal UI modal, clicks "Save", and the frontend **publishes directly** to the MQTT command topic. LINK accepts them and stores them in NVS. The portal is only transport.

This reduces compromise risk: a portal database leak does not grant access to the user's printers or HA.

### 2. One MQTT command topic for all three

```
idryer/<serial>/commands/link_integration
```

The payload is a discriminated union on the `type` field. Each time the modal is saved, the portal publishes only the payload for its own integration. LINK merges it into NVS on top of the existing data.

### 3. One active at a time

All three parameter sets are stored in NVS. But LINK **starts a client only for the active integration** - the user selects it in the menu.

### 4. One connection, two modes depending on `deviceType`

The Bambu/Moonraker connection is the same, but what LINK does with the data stream depends on the device type (`HelloPayload.deviceType`, in MQTT `info.deviceType`):

| `deviceType` | Mode | Bambu | Moonraker |
|--------------|------|-------|-----------|
| `Dryer` (0x01) / `Unknown` (legacy) | **Writer** - RFID tag emulation | LINK **writes** to the printer: `ams_filament_setting` via `bambu_apply` | (MVP not used) |
| `Heater` (0x02) / `LinkII` (0x05) | **Reader** - read status for chamber heating control | LINK **reads** `device/<serial>/report` → sends to MCU (filament type + print status; MCU decides chamber temperature using the local table) | LINK **reads** `gcode_macro VIRTUAL_CHAMBER` (`target` + `temperature` + `has_sensor`) → sends to MCU; target = setpoint, temperature = feedback for PID (if `hasSensor`) |

The configuration contract (`commands/link_integration`) is **the same** for both modes. The difference is that:

- The portal backend sends `commands/bambu_apply` **only for Dryer**. It does not send it for Heater/LinkII.
- After receiving the configuration, the LINK firmware starts either the **writer** or the **reader** depending on its own `deviceType`.

This allows one dryer (iDryer) and one heater (iHeater) to use the same Bambu LAN connection from different points in the room, for different tasks, with the same credentials in different NVS instances.

In MCU+LINK configuration: the menu is on the MCU, and `activeIntegration` reaches LINK through the usual `ConfigPush` (the menu item is just another element).

In standalone configuration (LINK without MCU): the menu is on LINK itself, and the value is stored locally.

Values:

| `activeIntegration` | What LINK does |
|---------------------|----------------|
| `"none"` | All clients are disabled (default) |
| `"ha"` | Starts `HaMqttClient`, publishes discovery + state |
| `"bambu"` | Starts `BambuClient`, connects to the printer over LAN MQTT |
| `"moonraker"` | Starts a WebSocket client to the Klipper host |

Changing the active integration at runtime is allowed. LINK must close the current connection correctly and open the new one.

### 4. Common status topic back

```
idryer/<serial>/integrations/status
```

LINK publishes a full snapshot of all integration states (retained, QoS 1). Any change in any field republishes it. The frontend uses this publication to render the status for all three modals at once.

---

## `commands/link_integration` Format

### HA

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

Field | Required | Notes
---|---|---
`type` | yes | `"ha"`
`enabled` | yes | true = the integration is enabled (but it only becomes active if selected in the menu)
`host` | no, default `homeassistant.local` | mDNS name or IP
`port` | no, default `1883` | TLS is not supported in MVP
`username` | no | empty = anonymous broker
`password` | no | used together with `username`
`discoveryPrefix` | no, default `"homeassistant"` | change if HA uses a non-standard prefix

### Bambu

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

Field | Required | Notes
---|---|---
`type` | yes | `"bambu"`
`enabled` | yes |
`ip` | yes | Printer IP on the local network
`serial` | yes | printer serial number, printed on the enclosure
`lanAccessCode` | yes | 8-digit code from the printer menu (Settings → LAN Mode)
`defaultAmsId` | no, default `255` | Default AMS slot (`255` = current active)
`defaultTrayId` | no, default `254` | Default tray (`254` = without AMS)
`autoApplyOnTagDetect` | no, default `true` | auto-apply on `tag_detected`

### Moonraker

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

Field | Required | Notes
---|---|---
`type` | yes | `"moonraker"`
`enabled` | yes |
`host` | yes | mDNS name or IP of the Klipper host
`port` | no, default `7125` | Moonraker standard
`apiKey` | no | `null` = open access; if Moonraker uses `[authorization]` with `force_logins`, a key is required
`ssl` | no, default `false` | `true` → `wss://`, otherwise `ws://`
`pollIntervalMs` | no, default `1000` | status poll interval

---

## `integrations/status` Format

```
Topic:    idryer/<serial>/integrations/status
QoS:      1
Retained: true
```

```json
{
  "active": "moonraker",
  "ha": {
    "configured": true,
    "enabled": true,
    "state": "idle",
    "host": "homeassistant.local",
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:00Z"
  },
  "bambu": {
    "configured": true,
    "enabled": false,
    "state": "idle",
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
    "printerState": "printing",
    "progress": 42,
    "remainingSeconds": 3600,
    "lastError": "",
    "updatedAt": "2026-04-19T12:00:12Z"
  }
}
```

### Common fields in each section

Field | Type | Description
---|---|---
`configured` | bool | NVS contains a non-empty valid parameter set
`enabled` | bool | `enabled: true` from the last `commands/link_integration`
`state` | string | See below
`lastError` | string | Human-readable error for the last operation, empty = OK
`updatedAt` | ISO 8601 | Time of the last status update

### `state` Values

Common to all integrations:

- `"disabled"` - `active != this type`, client disabled
- `"idle"` - active, settings exist, waiting for work
- `"connecting"` - connection attempt in progress
- `"online"` - connected and working
- `"config_missing"` - active, but parameters are empty or invalid
- `"error"` - error; see `lastError`

Bambu-specific fields: `printerIp`, `printerSerial`, `printerState`, `progress`, `remainingSeconds`, `lastApply` (result of the last `bambu_apply`).

Moonraker-specific fields: `host`, `printerState`, `progress`, `remainingSeconds`, `filename`, `currentLayer`, `totalLayers`.

Details are in the documents for each integration.

---

## Lifecycle (all three are the same)

```
1. The user opens the "Bambu" modal in the portal UI → enters IP/serial/code → Save.
2. Frontend publishes to MQTT:
      idryer/<serial>/commands/link_integration
      { "type": "bambu", "enabled": true, "ip": ..., ... }
3. LINK receives it, validates it, and stores the bambu section in NVS.
4. LINK publishes integrations/status with the updated bambu section.
5. While active != "bambu", LINK does not start BambuClient, but it knows the integration is configured.
6. The user changes active to "bambu" locally on the MCU screen or in the LINK menu.
7. The change reaches LINK through ConfigPush (or locally in standalone).
8. LINK closes the current client if there was one, and starts BambuClient with the saved parameters.
9. LINK publishes integrations/status with active="bambu" and state="connecting" → "online".
```

---

## How `active` is Set in the Menu

### MCU+LINK Configuration

A new MCU menu item registered in `menu_meta.h`:

- `id` - dedicated one (for example, `100`).
- `type` - enum with options `none/ha/bambu/moonraker`.
- The value is stored in MCU EEPROM like any other menu parameter.

The user changes it locally (encoder/screen) or remotely:

```
MQTT commands/set { "id": 100, "val": "moonraker" }
  → LINK sends ConfigPush with {"cmd":"set","id":100,"val":"moonraker"} to MCU
  → MCU applies it, increments rev, saves
  → MCU sends ConfigPush delta {"d":{"100":"moonraker"}} to LINK
  → LINK sees the change → switches the client
```

This flow is no different from any other menu item (see [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md)).

### Standalone LINK

The menu lives on LINK itself. The `activeIntegration` value is stored in LINK NVS together with the integration parameters. Changes go through a separate MQTT channel or through the local LINK UI (WebSocket, buttons, etc.).

---

## Security

| Risk | Mitigation |
|------|------------|
| Portal database leak | Credentials are not stored on the portal |
| MQTT command interception | TLS between the device and the iDryer broker |
| Compromise of the iDryer broker | There is no stronger assumption in the threat model - the broker is under iDryer control |
| Someone else publishes to `commands/link_integration` | EMQX checks `(serialNumber, deviceToken)`; only the device owner can publish (through the portal, which verifies ownership before publishing) |
| LINK logs leak the password | Mask all `password`/`lanAccessCode`/`apiKey` values in logs (print `****`) |

---

## What's Next

- [04-home-assistant.md](04-home-assistant.md) - HA integration details (publisher + mDNS + discovery).
- [05-bambu-integration.md](05-bambu-integration.md) - Bambu: config + apply on `tag_detected` + status.
- [06-moonraker-printer.md](06-moonraker-printer.md) - Moonraker: WebSocket JSON-RPC, printer status.
