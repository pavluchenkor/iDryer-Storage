# `idryer-protocol` documentation

The **`idryer-protocol`** library covers communication inside the iDryer ecosystem: UART between the controller and the network module, MQTT to the cloud, HTTP to the portal, and command parsing. It is intended for **developers building their own device compatible with the iDryer cloud**.

This documentation is a single guide. Read it in order if this is your first time here, or jump straight to the section you need if you are looking for a specific reference.

---

## How to Read This Documentation

The documents are numbered. Entry point on the left, depth on the right.

```
01-overview/          - Core concepts: what this is, system nodes, terminology
02-getting-started/   - Quick start for your development path
03-uart/              - The full UART protocol
04-mqtt/              - The MQTT contract with the cloud
05-cloud/             - Portal HTTP API and claiming
06-flows/             - End-to-end flows
07-features/          - RFID, local WebSocket
```

If this is your first time here, start with [01-overview/01-what-is-idryer.md](01-overview/01-what-is-idryer.md) and read in order. The sections are designed so that each one depends only on the previous ones.

!!! tip "Not enough background in C/C++, MQTT, or web basics?"
    Read [01-overview/05-prerequisites.md](01-overview/05-prerequisites.md) - a 10-minute mini tutorial on everything the documentation assumes you already know (bit operations, little-endian, `#pragma pack`, `enum class`, MQTT QoS/retained/keepalive/LWT, mDNS, JWT, and similar topics). Without that foundation, section [03-uart/](03-uart/) can easily look like encrypted text.

---

## Documentation Map

### 01. Overview

| Document | About |
|----------|-------|
| [01-what-is-idryer.md](01-overview/01-what-is-idryer.md) | What the product is, who the library is for, the three developer paths |
| [02-architecture.md](01-overview/02-architecture.md) | Three nodes (MCU / LINK / Cloud), three communication channels |
| [03-nodes-and-roles.md](01-overview/03-nodes-and-roles.md) | Who does what, where responsibility boundaries are drawn |
| [04-glossary.md](01-overview/04-glossary.md) | Glossary of terms and identifiers |
| [**05-prerequisites.md**](01-overview/05-prerequisites.md) | **Mini tutorial**: what you need to know about C/C++, MQTT, and web basics before reading the rest |

### 02. Getting Started

| Document | Your scenario |
|----------|--------------|
| [01-choose-your-path.md](02-getting-started/01-choose-your-path.md) | Choose one of the three development paths |
| [02-quickstart-controller.md](02-getting-started/02-quickstart-controller.md) | Your own MCU: manual frame assembly, CRC, hex dumps |
| [03-quickstart-bridge.md](02-getting-started/03-quickstart-bridge.md) | Your own ESP32 bridge built on the library (PlatformIO) |
| [04-quickstart-standalone.md](02-getting-started/04-quickstart-standalone.md) | One ESP32 does everything, full path to Online |
| [05-dev-claim.md](02-getting-started/05-dev-claim.md) | **How to claim a device to an account**: via screen, web installer, or Serial Monitor (dev) |

### 03. UART Protocol

| Document | Content |
|----------|------------|
| [01-physical-layer.md](03-uart/01-physical-layer.md) | Physical layer: 115200 8N1, wiring, level shifter |
| [02-frame-and-crc.md](03-uart/02-frame-and-crc.md) | Frame structure, flags, CRC-16/CCITT-FALSE with code |
| [03-message-types.md](03-uart/03-message-types.md) | Table of all `MessageKind` values and statuses |
| [04-binary-structures.md](03-uart/04-binary-structures.md) | Offsets, sizes, and types of every payload structure |
| [05-ack-retry.md](03-uart/05-ack-retry.md) | ACK, retry, fragmentation, idempotency, `_pad` |
| [06-examples.md](03-uart/06-examples.md) | 14 real hex dumps with verified CRC values |

### 04. MQTT

| Document | Content |
|----------|------------|
| [01-connection.md](04-mqtt/01-connection.md) | Connection, TLS/CA, credentials, LWT, keepalive, buffer |
| [02-topics.md](04-mqtt/02-topics.md) | Full topic table with QoS and retained |
| [03-device-to-backend.md](04-mqtt/03-device-to-backend.md) | JSON formats for device publications |
| [04-backend-to-device.md](04-mqtt/04-backend-to-device.md) | JSON commands from the cloud |
| [05-examples.md](04-mqtt/05-examples.md) | Ready-to-use `mosquitto_pub`/`sub` commands for debugging |

### 05. Cloud and HTTP

| Document | Content |
|----------|------------|
| [01-claiming-overview.md](05-cloud/01-claiming-overview.md) | Overview of device claiming to an account |
| [02-http-api.md](05-cloud/02-http-api.md) | Portal REST API contract with `curl` examples |
| [03-command-sink.md](05-cloud/03-command-sink.md) | `ICommandSink` for the UART bridge and standalone mode |

### 06. End-to-End Flows

| Document | Flow |
|----------|----------|
| [01-first-boot.md](06-flows/01-first-boot.md) | First boot: boot -> Hello -> WiFi -> claim -> MQTT |
| [02-claiming-flow.md](06-flows/02-claiming-flow.md) | Full claiming flow with UART frames |
| [03-remote-config.md](06-flows/03-remote-config.md) | Menu config exchange: full / delta / set / invoke |
| [04-profile-mode.md](06-flows/04-profile-mode.md) | Profile drying: PID, automatic RAMP/HOLD |

### 07. Optional Features

| Document | Feature |
|----------|------|
| [01-rfid.md](07-features/01-rfid.md) | `tag_detected` / `tag_removed` events, OpenPrintTag read/write |
| [02-websocket-local.md](07-features/02-websocket-local.md) | Local WebSocket channel without the cloud |
| [03-link-integrations-overview.md](07-features/03-link-integrations-overview.md) | **LINK integrations**: common contract for HA / Bambu / Moonraker *(design-level)* |
| [04-home-assistant.md](07-features/04-home-assistant.md) | Home Assistant: sensor publishing through mDNS |
| [05-bambu-integration.md](07-features/05-bambu-integration.md) | Bambu Lab: apply filament through LAN MQTT |
| [06-moonraker-printer.md](07-features/06-moonraker-printer.md) | Moonraker / Klipper: print status over WebSocket |
| [07-portal-integration-contract.md](07-features/07-portal-integration-contract.md) | **API reference for the portal developer** (all three integrations) |

---

## Standard Capabilities of All Devices in the Family

Regardless of `deviceType` (dryer / heater / telemetry / other future products):

- **Local MCU menu** is defined in `menu_meta.h`, shared by MCU and LINK.
- **Remote config** makes the same menu items available remotely through standard MQTT commands:
  - `commands/get_config` requests the full menu JSON.
  - `commands/set` changes a value.
  - `commands/invoke` executes an action.
  - Publications: `config` (full snapshot, retained) and `config/delta` (incremental update).
- **UI consequence:** the gear icon on the device card in the dashboard is a standard feature for all devices, not something specific to one device type.

Details: [06-flows/03-remote-config.md](06-flows/03-remote-config.md), [04-mqtt/04-backend-to-device.md](04-mqtt/04-backend-to-device.md).

---

## Quick Answers

- **I need my own controller compatible with iDryer. Where do I start?** -> [02-getting-started/02-quickstart-controller.md](02-getting-started/02-quickstart-controller.md).
- **How do I calculate UART CRC-16?** -> [03-uart/02-frame-and-crc.md](03-uart/02-frame-and-crc.md) (ready-to-use function in C and Python).
- **What exactly is inside `HelloPayload`, byte by byte?** -> [03-uart/04-binary-structures.md#hellopayload-0x01--86-байт](03-uart/04-binary-structures.md).
- **How do I publish telemetry to the correct topic?** -> [04-mqtt/02-topics.md](04-mqtt/02-topics.md) and [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md).
- **What is the difference between `unitId: 0` and `"U1"`?** -> [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md) (warning in the `info` section) and [01-overview/04-glossary.md](01-overview/04-glossary.md).
- **What is the provision request API URL?** -> [05-cloud/02-http-api.md](05-cloud/02-http-api.md).
- **The device is offline. What does the broker publish?** -> `idryer/<serial>/offline` with body `{}`. LWT setup: [04-mqtt/01-connection.md](04-mqtt/01-connection.md).

---

## Known Limitations (Release Checklist)

If you are building a product on top of the library, check which of these limitations matter for your scenario. Details are in the linked sections.

### Partially Implemented

| Limitation | Where described |
|-------------|-------------|
| Fragmentation of outgoing `ConfigPush` from MQTT `set`/`invoke`: not implemented for JSON > 194 bytes | [03-uart/03-message-types.md](03-uart/03-message-types.md), [06-flows/03-remote-config.md](06-flows/03-remote-config.md) |

### Not Implemented (Contract Only)

| Limitation | Where described |
|-------------|-------------|
| Publication of `Log` (0x60) to MQTT `events`: not automatic, requires `setLogHandler()` in application code | [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md), [03-uart/04-binary-structures.md](03-uart/04-binary-structures.md) |
| **LINK integrations** (HA credentials, Bambu config+apply, Moonraker status): the contract is ready, the LINK side is not implemented | [07-features/03-link-integrations-overview.md](07-features/03-link-integrations-overview.md), [07-features/07-portal-integration-contract.md](07-features/07-portal-integration-contract.md) |

### Design Limitations Worth Knowing

| Limitation | Where described |
|-------------|-------------|
| The reference `UartBridge` does not reset the parser on inter-byte timeout, only on CRC/size errors | [03-uart/05-ack-retry.md](03-uart/05-ack-retry.md) |
| `UartBridge` does not deduplicate by SEQ, so retried frames are processed twice | [03-uart/05-ack-retry.md](03-uart/05-ack-retry.md) |
| The `Heartbeat.wifiRssiDbm` field is overloaded: MCU -> LINK firmware may write temperature x 10 there | [03-uart/04-binary-structures.md](03-uart/04-binary-structures.md) |
| `unitId` is asymmetric: in `info` it is a number 0-3, in other topics it is the string `"U1"`...`"U4"` | [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md) |
| `readerId` in MQTT remains a **number** 0-3 (unlike `unitId`/`sensorId`, which are strings) | [04-mqtt/03-device-to-backend.md](04-mqtt/03-device-to-backend.md) |
| `startStage` in the `profile` command is not validated by LINK against `stages.length`; the client must check it | [04-mqtt/04-backend-to-device.md](04-mqtt/04-backend-to-device.md) |
| The overall claim timeout ("how long to wait for PIN entry") is not fixed in the library and is up to the application firmware | [05-cloud/01-claiming-overview.md](05-cloud/01-claiming-overview.md) |

---

## Sources of Truth

The documentation has been cross-checked against the following key files. If this documentation conflicts with the code, **the code wins**:

- `src/uart/uart_protocol.h`: all UART structures, enums, sizes.
- `src/uart/uart_protocol.cpp`: CRC16 implementation.
- `src/uart/uart_bridge.cpp`: parser, ACK/retry.
- `src/mqtt/idryer_topics.h`: topic constants, QoS, retained.
- `src/mqtt/mqtt_client.cpp`: connection, LWT, `publishInfo`.
- `src/cloud/telemetry_publisher.cpp`: JSON publication formats.
- `src/cloud/command_handler.cpp`: MQTT command parsing.
- `src/cloud/cloud_state_machine.cpp`: cloud state machine.
- `src/cloud/command_sink.h` / `uart_command_sink.h`: `ICommandSink`.

---

## External Documents

These materials live **outside** this repository, but they may still be useful:

- **iDryer Link repository**: [github.com/pavluchenkor/idryer-link](https://github.com/pavluchenkor/idryer-link) - reference consumer firmware built on top of this library.
- **iDryer Portal repository**: backend with claiming documentation (`docs/development/DEVICE_CLAIMING_PROTOCOL.md`, `LINK_CLAIM_SCENARIOS.md`, `PROVISION_SECURITY_DESIGN.md`). The exact path depends on your access to the iDryer infrastructure.
- **`BAMBU_EMULATION_PLAN.md`**: in the root of the `iDryerRP2040/` repository, the original design document for the Bambu integration (portal status as of April 2026).
- **`iHeater-link/virtual_chamber_guide.md`**: a validated user guide for iHeater that explains how to configure `[gcode_macro VIRTUAL_CHAMBER]` in Klipper for automatic chamber temperature control through `M141` in slicer start G-code.

---

## Archive

The `_legacy/` directory contains the old documentation structure, preserved for comparison during migration. It will be removed after the new version stabilizes.

---

## Feedback

Found a mismatch between the documentation and the code, an unclear phrasing, or an error in a hex dump? Open an issue in the `idryer-protocol` repository or contact the developer.

**Library author:** Ruslan Pavluchenko · `pavluchenko.r@gmail.com`
**License:** MIT
