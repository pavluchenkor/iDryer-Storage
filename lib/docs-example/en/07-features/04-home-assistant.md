# Home Assistant: Sensor Publishing Integration

LINK publishes device sensors (temperature, humidity, mode, weights) to the user's Home Assistant MQTT broker through the discovery mechanism. After that, HA automatically creates entities for each chamber and sensor.

!!! note "Status"
    - The **publisher (`HaMqttClient` + `HaPublisher`) exists** in the library code and works through mDNS with default settings.
    - **Passing HA parameters from the portal to LINK** (`commands/link_integration` with `type: "ha"`) is **design-level** and not implemented. This document fixes the target contract.
    - **Authorization (username/password)** and non-standard `host`/`port` are **not verified** in the current firmware; after `commands/link_integration` is implemented, this chain should work.

!!! warning "You are expected to know Home Assistant"
    This document does not explain what HA, the Mosquitto Add-on, discovery, or entities are. If you do not know them, first read the [official Home Assistant MQTT integration docs](https://www.home-assistant.io/integrations/mqtt/) and [MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery). Then come back here.

---

## Role

LINK is the **MQTT client of the user's HA instance**. The user does not change their HA installation - LINK appears as a device and publishes discovery on its own.

```
   iDryer LINK                    Home Assistant
  ┌──────────────┐               ┌────────────────┐
  │ HaMqttClient ├──MQTT──────►│ mosquitto      │
  │              │               │                │
  │ HaPublisher  │               │ HA core        │
  └──────────────┘               └────────────────┘
                                          │
                                          ▼
                                     ┌─────────┐
                                     │ iDryer  │ ← appears as device
                                     │ U1 Temp │ ← sensor entity
                                     │ U1 Hum  │
                                     │ U1 Mode │
                                     └─────────┘
```

---

## Configuration

### Default mode (mDNS, no credentials)

LINK resolves `homeassistant.local` through mDNS and connects to port `1883` without authentication. This works out of the box for users whose HA is reachable under that name and whose MQTT broker allows anonymous access.

If `active = "ha"` and the `ha` section in NVS is empty, LINK uses the default.

### Through `commands/link_integration`

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

Parameter | Default | Notes
---|---|---
`host` | `homeassistant.local` | mDNS name or IP |
`port` | `1883` |  |
`username` | empty | if filled, MQTT auth is used |
`password` | empty | used together with `username` |
`discoveryPrefix` | `"homeassistant"` | change if HA is configured with another prefix |

The common contract (one topic for all integrations, so user credentials do not reach the portal) is described in [03-link-integrations-overview.md](03-link-integrations-overview.md).

---

## Publishing to HA

### Discovery

When LINK connects, it publishes discovery messages to `homeassistant/<component>/<object_id>/config`:

- `homeassistant/sensor/idryer_<serial>_u1_temperature/config`
- `homeassistant/sensor/idryer_<serial>_u1_humidity/config`
- `homeassistant/sensor/idryer_<serial>_u1_heater_power/config`
- `homeassistant/binary_sensor/idryer_<serial>_u1_fan/config`
- ... for each chamber

The discovery payload contains:

- `name` - `"iDryer U1 Temperature"`.
- `state_topic` - `idryer/<serial>/ha/u1/temperature`.
- `device_class` - `temperature`, `humidity`, `power_factor`, etc.
- `unit_of_measurement` - `°C`, `%`, `%`.
- `device` - group: `{ identifiers: [serial], manufacturer: "iDryer", model: "Dryer", sw_version }`.

### State

LINK publishes values to separate state topics (QoS 0, retained) at the same interval as MQTT telemetry in the iDryer cloud. Typically:

- `idryer/<serial>/ha/u1/temperature` → `"55.3"`
- `idryer/<serial>/ha/u1/humidity` → `"45.2"`
- `idryer/<serial>/ha/u1/heater_power` → `"80"`
- `idryer/<serial>/ha/u1/fan` → `"ON"` / `"OFF"`

### Availability (planned)

LWT on the HA broker: `idryer/<serial>/ha/status` with payload `"offline"`. When LINK connects it publishes `"online"` (retained). Set `availability_topic` in each sensor discovery entry.

---

## Current implementation (library)

`HaMqttClient` in `src/mqtt/ha_mqtt_client.*`:

- The constructor accepts an optional `host`.
- `begin()` searches for HA through mDNS if `host` is not set.
- `publish(topic, payload, retained)` is the low-level publication method.

`HaPublisher` in `src/cloud/ha_publisher.*`:

- Accepts `HaMqttClient*`.
- Publishes discovery and sensor state.

**What is not implemented:**

- Handling `commands/link_integration` to store parameters in NVS and reconfigure `HaMqttClient`.
- Verified support for `username`/`password` on HA instances with authentication enabled.
- LWT/availability.
- Reaction to `active` changes in the menu: starting/stopping `HaMqttClient`.

---

## `integrations/status` for HA

The `ha` section in the shared status object ([03-link-integrations-overview.md](03-link-integrations-overview.md)):

```json
"ha": {
  "configured": true,
  "enabled": true,
  "state": "online",
  "host": "homeassistant.local",
  "brokerPort": 1883,
  "authUsed": true,
  "lastError": "",
  "updatedAt": "2026-04-19T12:00:00Z"
}
```

Additional fields:

- `brokerPort` - the actual connection port.
- `authUsed` - `true` if `username` + `password` were provided, otherwise `false` (anonymous mode).

The `password` value is never published in status.

---

## What's Next

- [03-link-integrations-overview.md](03-link-integrations-overview.md) - common command and status contract.
- [05-bambu-integration.md](05-bambu-integration.md) - Bambu integration (next in complexity).
- [06-moonraker-printer.md](06-moonraker-printer.md) - Moonraker/Klipper.
