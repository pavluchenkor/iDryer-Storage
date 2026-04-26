# System Architecture

This document explains **who talks to whom, over which channel, and under which rules**. After reading it, you should have a mental map: "these are the three nodes, these are the three communication channels, and this is what moves over each channel."

!!! note "Terms"
    All identifiers, abbreviations, and units are listed in [04-glossary.md](04-glossary.md). If you run into an unfamiliar acronym (NVS, LWT, RSSI), look there first.

## Three nodes

```
┌─────────────┐           ┌─────────────┐           ┌─────────────┐
│     MCU     │   UART    │    LINK     │   MQTT    │   Cloud     │
│ Controller  │◄─────────►│ Network module │◄────────►│  (portal)   │
└─────────────┘           └─────────────┘           └─────────────┘
      │                         │                         ▲
      │                         │                         │ HTTP
      │                         └─────────────────────────┘
      │                            (LINK -> Cloud only)
      │
      ▼
  Sensors,
  heater,
  screen, EEPROM
```

| Node | Physically | What it is responsible for |
|------|-----------|------------------|
| **MCU** | Microcontroller inside the dryer (typically RP2040, but any Arduino-compatible board works) | Hardware control, sensors, PID, screen, EEPROM |
| **LINK** | WiFi module (typically ESP32) running the `idryer-protocol` library | Cloud connectivity, UART <-> MQTT bridge, claiming |
| **Cloud** | iDryer server: portal REST API + MQTT broker + database | Device storage, account binding, command routing |

**Simplification:** for standalone devices (one ESP32 does everything), MCU and LINK are merged into one node. Architecturally, it is the same loop: the library works the same way inside.

## Three communication channels

### Channel 1: UART (MCU <-> LINK)

**Physical layer:** standard UART, 115200 baud, 8N1, no hardware flow control. Two wires for data (TX/RX) + GND. Optional power cable.

**What moves:**

- MCU -> LINK: telemetry, status, RFID events, Hello on startup, Heartbeat, application errors (`Log`).
- LINK -> MCU: cloud commands (Start/Stop/Profile/... ), config, claiming PIN, network status.
- Both sides: `Heartbeat` every 5 seconds, `ACK` for requests with the `ACK_REQUIRED` flag.

**Rules:**

- Every frame starts with byte `0xAA`, includes CRC16, and has a sequence number.
- Reliable requests are marked with `ACK_REQUIRED`. If no ACK arrives within 700 ms, retry up to 3 times.
- Payload length is up to 200 bytes. Larger data (config JSON) is fragmented.

Details: [../03-uart/](../03-uart/).

### Channel 2: MQTT (LINK <-> Cloud)

**Physical layer:** TLS connection to the portal MQTT broker (in production, port 8883, Let's Encrypt CA certificate).

**What moves:**

- LINK -> Cloud: telemetry, status, weights, RFID events, application events, menu config.
- Cloud -> LINK: user commands (start drying, stop, settings).

**Rules:**

- All topics start with `idryer/<serialNumber>/...`, where `serialNumber` is the LINK module identifier.
- The MQTT password is `deviceToken`, which LINK gets from the portal once on first connection. It is not the portal user password and not the WiFi password.
- QoS and retained are defined per topic (`info` is retained, `telemetry` is not retained, and so on).

Details: [../04-mqtt/](../04-mqtt/).

### Channel 3: HTTP (LINK -> Cloud)

**Physical layer:** HTTPS with the portal public API, `portal.idryer.org/api`.

**What moves** (the device claiming flow, "claiming" - details in [../05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md)):

- `POST /devices/provision` - get `deviceToken` for MQTT.
- `POST /devices/register` - get a PIN for the user.
- `GET /devices/check-claim/:token` - wait until the user claims the device in the app.

**Rules:**

- HTTP is needed **only on first boot** or after an NVS reset. In normal operation, the device lives in MQTT.
- Some endpoints do not require authentication (provision/register), and some require the user's JWT (claim is done by the app, not the device).

Details: [../05-cloud/](../05-cloud/).

## Who initiates communication

This is important to understand up front.

| Event | Initiator | Where it goes |
|---------|-----------|-------------|
| Device startup | MCU -> Hello -> LINK | LINK triggers MCU if needed |
| Telemetry | Sent by MCU itself | LINK publishes to MQTT |
| User command (Start/Stop/...) | User in app -> Cloud -> LINK -> MCU | Always down the chain |
| Device claiming | MCU -> ClaimStart -> LINK | LINK goes to HTTP |
| Heartbeat | Both sides (MCU and LINK) | Crosswise |

**Key point:** the cloud **cannot** call the controller directly. Always through LINK, always through MQTT. If LINK is offline, the device cannot be controlled remotely, but it still works locally.

## Chambers (units)

The dryer can have **1-4 chambers** (`unit`). Each chamber is an independent loop: its own heater, its own sensors, its own operating mode.

- `unitId` in UART: `0, 1, 2, 3`.
- In MQTT, the same index is represented as a string: `U1, U2, U3, U4`.
- The special `0xFF` value in UART (`unitId = 0xFF`) means "all chambers" - for commands such as `stop`.

The chamber configuration (which sensors and which RFID readers are attached) is sent once at startup in the `HelloPayload` structure.

## Identifiers

Several identifiers exist in the system, and it is important not to mix them up:

| Identifier | Belongs to | Example | Where it is used |
|----------------|-----|--------|-------------------|
| `serialNumber` | LINK | `DEVICE_aabbccddeeff_1234567` | Prefix for MQTT topics, username for the broker |
| `mcuSerial` | MCU | `36B955AB4350` (16 hex) | Sent inside Hello, forwarded to MQTT info |
| `deviceToken` | LINK | secret string | MQTT password, stored in the module NVS |
| `deviceId` | Device in the portal DB | UUID | Used only for display |
| User JWT | Portal user | JWT | For `POST /devices/claim` from the app, **not** for the device |

Full definitions are in [04-glossary.md](04-glossary.md).

## What next

- [03-nodes-and-roles.md](03-nodes-and-roles.md) - detailed breakdown: what each node does.
- [04-glossary.md](04-glossary.md) - full glossary of terms.
- [../02-getting-started/01-choose-your-path.md](../02-getting-started/01-choose-your-path.md) - choose your path.
