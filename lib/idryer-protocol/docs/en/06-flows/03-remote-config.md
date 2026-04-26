# Remote config: menu settings through the cloud

The user opens the app, changes a setting in the device menu, and the value must go to the MCU, be applied, be saved in EEPROM, and come back as an acknowledgement. The reverse also happens: the device sends the full config to the cloud so the app can show the current values.

The MCU is the source of truth for settings. LINK only forwards them.

---

## Core concepts

- **Full config** — the entire menu JSON (`{"v":8,"units":3,"active":0,"lang":"en","menu":[...]}`), up to about 3 KB. Published to `idryer/<serial>/config`.
- **Delta** — an incremental change for one element (`{"d":{"3":[55,60,55]}}`). Published to `idryer/<serial>/config/delta`.
- **set** — a cloud command: “change parameter with ID=3 to value X”.
- **invoke** — a cloud command: “run action with ID=5” (for example, “save to EEPROM”, “restore factory defaults”).
- **UART `ConfigPush` (0x30)** — the transport for all three: full JSON, delta, set/invoke.
- **rev** — the config revision number on the MCU, incremented on every change; it guarantees consistency.

---

## Scenario 1: the device publishes the full config

When this happens:

- The user opened the device app for the first time.
- The app sent `commands/get_config`, and the device responds.
- The firmware was updated, so the menu may have changed.

### Sequence

```
App  ──MQTT publish commands/get_config──►  LINK
LINK ──UART Command{GetConfig}──►  MCU
MCU  ──UART ConfigPush (fragmented)──►  LINK
     [fragment 1 of N, FLAG_FRAGMENTED]
     [fragment 2 of N, FLAG_FRAGMENTED]
     ...
     [fragment N, FLAG_LAST_FRAGMENT]
LINK ──assembles the full JSON──►  ...
LINK ──MQTT publish idryer/<serial>/config──►  Broker
```

Fragmentation: each chunk is `ConfigChunkPayload` (6-byte header + up to 194 bytes of data). See [../03-uart/05-ack-retry.md](../03-uart/05-ack-retry.md) and [../03-uart/06-examples.md](../03-uart/06-examples.md) for details.

### Full JSON format

```json
{
  "v": 8,
  "units": 3,
  "active": 0,
  "lang": "en",
  "menu": [
    { "id": 3, "t": "val", "val": [50, 65, 85] },
    { "id": 5, "t": "val", "val": [40, 15] },
    { "id": 17, "t": "val", "val": [55, 60, 55] }
  ]
}
```

- `v` — the config schema version and/or current `rev`.
- `units` — the number of active chambers.
- `active` — the index of the current active chamber (0-based).
- `lang` — `"en"` / `"ru"`.
- `menu[]` — the list of menu items. The `val` field can be a number or an array (one item per chamber).

Which IDs belong to which items and their types are defined in `menu_meta.h`, shared by the MCU and LINK. This synchronization is ensured by building both firmware images from the same library repository.

---

## Scenario 2: the user changes a value (set)

In the app, the user selects “target chamber temperature U1 = 55 °C”.

```
App  ──MQTT publish commands/set──►  LINK
     { "id": 3, "unit": 0, "val": 55 }

LINK: parser → CommandHandler.handleSet → sink.sendConfigPushChunk(...)

LINK ──UART ConfigPush──►  MCU
     data = {"cmd":"set","id":3,"unit":0,"val":55}
     (if JSON <= 194 bytes, one frame with FLAG_LAST_FRAGMENT)

MCU: applies it, increments rev, saves to EEPROM.

MCU  ──UART ConfigPush (delta)──►  LINK
     data = {"d":{"3":[55,60,55]}}

LINK ──MQTT publish idryer/<serial>/config/delta──►  Broker
     {"d":{"3":[55,60,55]}}
```

The delta contains only changed elements; if one cell in an array changes, the MCU still sends the full array for that ID so the app sees the current complete value.

---

## Scenario 3: the user invokes an action (invoke)

For example, “restore factory settings for the chamber”.

```
App  ──MQTT publish commands/invoke──►  LINK
     { "id": 7 }

LINK ──UART ConfigPush──►  MCU
     data = {"cmd":"invoke","id":7}

MCU: performs the action, increments rev, and publishes the final delta for all changes.

MCU  ──UART ConfigPush (delta)──►  LINK
     data = {"d":{"3":[50,50,50],"5":[40,15],"17":[55,55,55]}}

LINK ──MQTT publish idryer/<serial>/config/delta──►  Broker
```

For an `invoke` that affects many items, the delta can be large. If it exceeds 194 bytes, fragmentation would be needed. In the reference MCU firmware, its deltas are fragmented correctly (`UartBridge.sendConfigPushChunk`).

---

## Limits

### Large set/invoke payloads from MQTT

If the cloud JSON (`{"cmd":"set","id":9,"unit":0,"val":[1,2,3,…]}`) exceeds `CONFIG_CHUNK_DATA_SIZE = 194` bytes, the reference LINK firmware **does not fragment** the outgoing `ConfigPush`; it logs “fragmentation not implemented”. This is a known limitation. For normal settings (numbers and short arrays), the limit is sufficient.

### Large configs from MCU to cloud

The MCU fragments correctly. MQTT `config` is published as a single complete message. The MQTT buffer limit is 16 KB (see [../04-mqtt/01-connection.md](../04-mqtt/01-connection.md)). A full config for 4 chambers rarely exceeds 3 KB.

### Identifiers and semantics

The menu item `id` is a flat index defined in `menu_meta.h`. When new items are added in firmware, only this header changes. Versioning through `v` or `rev` helps the app detect a mismatch and request the full config again.

---

## Config versioning

```
Event          rev       Action
──────────────  ──────    ──────────────────────────────────
boot            loaded from EEPROM
MQTT set        rev++     delta published
MQTT invoke     rev++     delta published
OTA update      reset     full config via GetConfig
```

If the app has been offline for a long time and, after reconnecting, receives a retained `config` with a different `v`/`rev`, it must request a full `get_config` again.

---

## Next steps

- [04-profile-mode.md](04-profile-mode.md) — profile drying (a separate command type).
- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) — JSON for `set`/`invoke`/`get_config` commands.
- [../03-uart/05-ack-retry.md](../03-uart/05-ack-retry.md) — fragmentation mechanics.
