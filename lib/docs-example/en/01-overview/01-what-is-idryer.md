# What iDryer is and why this library exists

!!! note "Before reading"
    The terms used in this documentation are listed in [04-glossary.md](04-glossary.md). Keep that tab open; some concepts are introduced gradually, and the full definitions are always there.

## The iDryer product

**iDryer** is a filament drying system for 3D printing. Physically, it is a cabinet with one or more chambers. Each chamber contains a heater, a fan, temperature and humidity sensors, and optionally load cells for spool weighing and RFID readers for spool identification.

The user controls drying:

- locally, through the screen and on-device menu,
- remotely, through the app or the web portal at [portal.idryer.org](https://portal.idryer.org).

Under the hood:

- **Controller** (MCU - MicroController Unit, the dryer microcontroller) controls the hardware: reads sensors, regulates heating with PID, drives the screen and menu, and stores settings in EEPROM.
- **Network module** (LINK - a proper name, not an abbreviation) gives the device WiFi, connects it to the iDryer cloud, receives server commands, and publishes telemetry.
- **Cloud** (portal + MQTT broker + backend) is the shared infrastructure for all iDryer devices.

## Why this library exists

**`idryer-protocol`** is a C++ library that covers everything related to connectivity:

- **UART protocol** between the controller and the network module (binary frames, CRC, ACK/retry).
- **MQTT client** for the network module (broker connection, telemetry publishing, command reception).
- **HTTP client** for claiming the device to a user account through the portal's public API.
- **Command parser** for cloud commands and their normalization into UART packets.

The library is intended for **a developer who is building a device compatible with the iDryer cloud**.

## Who this library is for

Three common paths:

| What you are building | What you use from the library |
|------------|--------------------------------|
| Your own controller (your own board, STM32 / RP2040 / ESP32 / other) | UART protocol only. A **ready-made** network module talks to the cloud. |
| Your own network module on ESP32 | MQTT + HTTP + UART bridge. The controller is yours or from a third party. |
| Standalone device (one ESP32 does everything) | Everything: UART structures (natively), MQTT, HTTP, command parser. |

**Not intended** for:

- building your own portal or your own MQTT broker,
- replacing the app for an end user.

If your goal is a fully autonomous ecosystem without the iDryer cloud, you do not need this library. Use its UART section as a format example and build your own.

## Responsibility boundaries

The library does **not** do the following - that is the job of the application firmware:

- PID heating control, sensor handling, and drying logic.
- Screen, menu, and encoder/button input.
- Storing user settings in EEPROM.
- Deciding **when** to publish to MQTT (the library provides the API; the application code sets the cadence).
- Automatic publication of `Log` from UART to MQTT `events` - this happens only if the application code registers the corresponding callback. See [03-nodes-and-roles.md](03-nodes-and-roles.md) for why this is done that way.

The library **does**:

- Pack and unpack UART frames with CRC and ACK.
- Connect to WiFi, make HTTP calls to the iDryer portal, and provide an MQTT client.
- Publish telemetry, status, weights, and RFID data to standard iDryer topics.
- Parse MQTT commands (`drying`, `stop`, `profile`, ...) and convert them into UART packets for the controller.

## Architecture at a glance

```
┌──────────────────┐   UART    ┌──────────────────┐   MQTT (TLS)   ┌──────────────────┐
│   Controller     │◄─────────►│  Network module  │◄──────────────►│   iDryer cloud   │
│                  │           │                  │                │                  │
│  • PID / heater  │           │  • WiFi          │                │  • portal API    │
│  • sensors       │           │  • MQTT client   │                │  • MQTT broker   │
│  • screen / menu │           │  • HTTP client   │                │  • web + app UI  │
│  • EEPROM        │           │  • claiming flow │                │                  │
└──────────────────┘           └──────────────────┘                └──────────────────┘
       (your code)                  (library)                         (ready-made)
```

In this documentation, the controller is called **MCU**, the network module is **LINK**, and the cloud is **portal** or **backend**. The full list of terms is in [04-glossary.md](04-glossary.md).

## What next

1. Read more about **nodes and their roles** - [03-nodes-and-roles.md](03-nodes-and-roles.md).
2. Read more about **communication architecture** - [02-architecture.md](02-architecture.md).
3. Terms, identifiers, and units - [04-glossary.md](04-glossary.md).
4. Quickstart for a specific scenario - [../02-getting-started/01-choose-your-path.md](../02-getting-started/01-choose-your-path.md).

---

## Acknowledgments

The documentation structure and its adaptation for beginner developers were created in co-authorship with **Claude Sonnet** (Anthropic). Thanks for help with structuring the material, choosing examples, finding places where the author assumed knowledge the reader may not have, and writing the mini tutorial in [05-prerequisites.md](05-prerequisites.md).☺️
