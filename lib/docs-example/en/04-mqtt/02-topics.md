# MQTT: Topics

Complete list of iDryer topics. The prefix for all of them is `idryer/<serialNumber>/`.

!!! note "Source of truth"
    `src/mqtt/idryer_topics.h` - topic names, QoS, retained flags, and intervals.

---

## Prefix

```
idryer/<serialNumber>/<suffix>
```

Example: for a device with `serialNumber = DEVICE_aabbccddeeff_1234567`, the telemetry topic is `idryer/DEVICE_aabbccddeeff_1234567/telemetry`.

---

## Device → Backend

| Suffix | QoS | Retained | Interval | Purpose |
|---------|-----|----------|----------|------------|
| `info` | 1 | **yes** | once | Static information: versions, units, capabilities, mcuSerial |
| `telemetry` | 0 | no | ~5 s | Temperature, humidity, heater, fan |
| `status` | 1 | **yes** | on change | Operating mode, timers, session |
| `weights` | 1 | no | ~10 s / on event | Filament weights |
| `rfid` | 1 | **yes** | on event | tag_detected / tag_removed, plus tag data |
| `events` | 1 | no | on event | App logs and errors |
| `config` | 1 | no | on request | Full menu JSON (~3 KB) |
| `config/delta` | 1 | no | on change | Menu delta update |
| `offline` | 1 | no | - | Broker LWT on unexpected disconnect |
| `integrations/status` | 1 | **yes** | on change | LINK integration status (HA/Bambu/Moonraker). *Design-level, not implemented.* |

**Intervals** are approximate. The actual frequency is set by application code:

- UART from MCU arrives at 1 s (active) / 15 s (idle).
- In MQTT, `IDRYER_INTERVAL_TELEMETRY_MS = 5000` is the recommendation for how often to duplicate the last value to the broker.

### Retained: what a new client sees

`info`, `status`, `rfid` are retained. A new app subscribing to these topics receives the latest messages immediately, even if the device is not publishing at that moment. This lets the UI open the device page without delay.

`telemetry`, `weights`, `events`, `config`, `config/delta` are not retained. A new client waits for the next publication.

---

## Backend → Device

All commands go to the `idryer/<serial>/commands/#` subscription. The suffix is the command name.

| Topic | QoS | Purpose |
|-------|-----|------------|
| `commands/drying` | 1 | Start normal drying |
| `commands/storage` | 1 | Start storage mode |
| `commands/profile` | 1 | Start profile drying |
| `commands/stop` | 1 | Stop |
| `commands/find` | 1 | Find the device (blink) |
| `commands/get_config` | 1 | Request full menu JSON |
| `commands/set` | 1 | Change a menu parameter |
| `commands/invoke` | 1 | Invoke an action from the menu |
| `commands/read_rfid` | 1 | Start reading an RFID tag |
| `commands/write_rfid` | 1 | Start writing an RFID tag (stop-and-wait ACK flow control) |
| `commands/clear_errors` | 1 | Clear the EEPROM error log |
| `commands/ping` | 1 | Liveness check (log only) |
| `commands/link_integration` | 1 | HA/Bambu/Moonraker settings (*design-level, not implemented*) |
| `commands/bambu_apply` | 1 | Apply filament to Bambu on `tag_detected` (*design-level*) |

The JSON format of each command is in [04-backend-to-device.md](04-backend-to-device.md).

---

## Static reference (C constants)

```c
// From src/mqtt/idryer_topics.h
#define IDRYER_TOPIC_PREFIX          "idryer"
#define IDRYER_TOPIC_INFO            "info"
#define IDRYER_TOPIC_TELEMETRY       "telemetry"
#define IDRYER_TOPIC_STATUS          "status"
#define IDRYER_TOPIC_WEIGHTS         "weights"
#define IDRYER_TOPIC_RFID            "rfid"
#define IDRYER_TOPIC_EVENTS          "events"
#define IDRYER_TOPIC_CONFIG          "config"
#define IDRYER_TOPIC_CONFIG_DELTA    "config/delta"
#define IDRYER_TOPIC_OFFLINE         "offline"

#define IDRYER_TOPIC_CMD_DRYING      "commands/drying"
#define IDRYER_TOPIC_CMD_STOP        "commands/stop"
#define IDRYER_TOPIC_CMD_STORAGE     "commands/storage"
#define IDRYER_TOPIC_CMD_FIND        "commands/find"
#define IDRYER_TOPIC_CMD_GET_CONFIG  "commands/get_config"
#define IDRYER_TOPIC_CMD_SET         "commands/set"
#define IDRYER_TOPIC_CMD_INVOKE      "commands/invoke"
#define IDRYER_TOPIC_CMD_READ_RFID   "commands/read_rfid"
#define IDRYER_TOPIC_CMD_WILDCARD    "commands/#"
```

!!! note "Commands without dedicated constants"
    `profile`, `write_rfid`, `clear_errors`, `ping` - their constants are missing from the header, but `CommandHandler::handleMqttCommand` recognizes these names (the last segment of the topic). Use explicit strings when building the topic, or add your own `#define`s in application code.

---

## Home Assistant (separate prefix)

If you plan to integrate the device with Home Assistant, the library provides a separate layer (`HaMqttClient`, `HaPublisher`) with the `homeassistant/...` prefix. This is a separate track from `idryer/...`. It is not covered in this documentation; sources are `src/mqtt/ha_mqtt_client.*`, `src/cloud/ha_publisher.*`.

---

## Next

- [03-device-to-backend.md](03-device-to-backend.md) - JSON publication formats.
- [04-backend-to-device.md](04-backend-to-device.md) - JSON command formats.
