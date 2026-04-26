# Nodes and Roles

Detailed breakdown: **what each node does, what goes into it and out of it, and what it owns**. After reading this, you should clearly understand what part of the system is your code, and what part is the library and the cloud.

!!! note "Terms"
    All terms are listed in [04-glossary.md](04-glossary.md).

## MCU (Controller)

### Who it is

The microcontroller inside the dryer. In the reference iDryer firmware, it is an RP2040. But the library is **not tied** to a specific chip: the UART section works on any Arduino-compatible hardware (STM32, AVR, ESP32-C3, and so on).

### Goal

Control the dryer hardware and provide the local UX.

### Responsibilities

- Read temperature and humidity sensors.
- Regulate the heater (PID).
- Run the drying session (Drying / Storage / Profile / Fault / Idle modes).
- Read RFID tags and load cells, if present.
- Show the menu and current state on the screen.
- Store user settings in EEPROM.
- Initiate device claiming based on a user action (the "start claiming" button).

### What MCU gets from the library

UART frame structures and their definitions (`idryer_protocol.h`). This is a **header-only** section - no runtime. You include `idryer_protocol.h` in your project and get ready-made `struct` and `enum` types with `#pragma pack(1)` and `static_assert` size checks.

### What MCU must be able to do

Minimum:

- Send `Hello` on startup after receiving `HelloAck`.
- Periodically send `Telemetry` and `Heartbeat`.
- Accept `Command` and respond with `CommandAck`.
- Respond to a LINK trigger with a `HelloAck`-like message (`Role = HelloRequest`).

Optional:

- `Weights`, `Rfid` - if the corresponding hardware exists.
- `Status` - if you want the cloud to see the operating mode and timers.
- `ConfigPush` - remote menu config.
- `ClaimStart` - device claiming.
- `Log` - structured events.
- `WsEnable` - local WebSocket access without the cloud.

## LINK (Network module)

### Who it is

An ESP32 network module. The `idryer-protocol` library runs on it. In the reference iDryer product, this is a separate board that communicates with the MCU over UART.

### Goal

Connect the MCU to the iDryer cloud and make that connection reliable.

### Responsibilities

**UART bridge:**

- Receive frames from the MCU, validate CRC, and reply with ACK when needed.
- Parse frames and invoke application callbacks.
- Send frames to the MCU.

**WiFi:**

- Connect to an access point (SSID/password from firmware or provision).
- Detect connection loss and reconnect.

**HTTP (initial claiming):**

- `POST /devices/provision` -> obtain `deviceToken`.
- `POST /devices/register` -> obtain a PIN for the user.
- `GET /devices/check-claim/:token` -> wait for claim.

**MQTT:**

- Connect to the broker with `serialNumber` as the username and `deviceToken` as the password.
- Publish telemetry, status, weights, RFID, and config.
- Subscribe to `idryer/<serial>/commands/#`.
- Handle incoming commands and convert them into UART frames.

**Claiming state machine:**

- Manage `WifiConnecting -> Provisioning -> Registering -> AwaitingClaim -> Ready -> MqttConnecting -> Online`.
- Send the current state to the MCU through `Heartbeat.cloudState`.

### What LINK gets from the library

Ready-made components (you do not need to memorize each one - they will be covered in detail in the quickstarts and protocol sections; here is just the list and the relationship diagram):

- `DryerUart::UartBridge` - UART frame parsing and construction.
- `idryer::MqttClient` - MQTT client.
- `idryer::HttpApi` - portal HTTP client.
- `idryer::cloud::CommandHandler` - command parsing.
- `idryer::cloud::CloudStateMachine` - state machine.
- `idryer::TelemetryPublisher` - JSON generation from UART payloads.

**How they are connected:**

```
                        (incoming UART frames)
                                   ▼
                         ┌─────────────────┐
                  UART ──┤   UartBridge    ├── callbacks ──► application code
                         └─────────────────┘
                                   ▲
                                   │ uart.sendCommand(...)
                                   │
                         ┌─────────────────┐
                         │ UartCommandSink │◄── sink ──┐
                         │  (implementation │          │
                         │   of ICommandSink)│          │
                         └─────────────────┘           │
                                                        │
  incoming MQTT command                                │
         │                                              │
         ▼                                              │
┌─────────────────┐     JSON    ┌─────────────────┐    │
│   MqttClient    │────────────►│ CommandHandler  │────┘
└─────────────────┘             └─────────────────┘
         ▲                              
         │ publish telemetry/status
         │                              
┌─────────────────┐   UART payload   ┌──────────────────┐
│TelemetryPublisher│◄───────────────│   UartBridge     │
└─────────────────┘   (callback)      │ (same inputs)    │
                                      └──────────────────┘

┌─────────────────┐     HTTP     ┌──────────────────┐
│    HttpApi      │◄────────────►│ CloudStateMachine│
└─────────────────┘              └──────────────────┘
                                          │
                                          │ cloudState -> into MCU Heartbeat
                                          ▼
                                     UartBridge
```

`UartCommandSink` is a ready-made "transition" implementation: `CommandHandler` parses JSON and calls `sink->sendCommand(...)`, and the sink turns that into a UART call through `UartBridge`. For devices without UART (standalone on a single ESP32), you can write your own `ICommandSink` implementation that applies the command locally - see [../05-cloud/03-command-sink.md](../05-cloud/03-command-sink.md) for details.

The application code creates instances of these classes, passes them into one another through constructors (this is what the quickstart calls "assemble the chain"), registers callbacks for incoming UART frames and MQTT commands, and then starts the main loop.

## Cloud (Portal)

### Who it is

The ready-made iDryer infrastructure: a Node.js server (NestJS), database, MQTT broker (EMQX), web app, and mobile app. The public entry point is `portal.idryer.org`.

### Goal

Bind devices to user accounts and act as the intermediary between the user UI and the devices.

### Responsibilities

- Accept provision requests from new devices and issue `deviceToken`.
- Accept register requests and issue a PIN.
- Accept claim requests from user apps and bind the device to the account.
- Accept telemetry from devices and show it in the UI.
- Accept user commands in the UI and publish them to the device MQTT topics.
- Keep track of devices and users in the database.

### What Cloud provides to a device developer

- A public HTTP API (no authentication for provision/register).
- An MQTT broker that verifies `(serialNumber, deviceToken)` through an HTTP auth hook.
- A set of topics with fixed prefixes and names.
- JSON formats for telemetry and commands.

### What Cloud **does not** do

- It does not know how your controller is built internally.
- It does not force its own UI - you can use the web portal, the app, or third-party clients.
- It does not store user menu settings - they live on the MCU.
- It does not know the LINK or MCU firmware - it only interacts through the contract.

## Boundary between "library" and "application firmware for LINK"

This is a common source of confusion. The boundary below is strict.

### What the library does

- Build and parse UART frames.
- MQTT client: connection, LWT, subscription, publication.
- HTTP client: provision / register / check-claim.
- `CommandHandler` - parse incoming MQTT commands and **convert** them into UART structures (not send them - see below).
- `CloudStateMachine` - connection state machine.
- `TelemetryPublisher` - build publication JSON from UART payloads.

### What the LINK application firmware must do

- Create component instances and connect them (dependency injection).
- Implement `ICommandSink` - "where to send parsed commands". For the classic UART configuration, the library provides a ready-made `UartCommandSink` (it sends through `UartBridge`).
- Register callbacks for incoming UART frames (what to do on `Telemetry`, `Log`, `ClaimStart`, ...).
- Decide **when** to publish. For example, `Log` (0x60) on UART is **not** published to MQTT automatically - you need a callback that calls `MqttClient::publishEvent`.
- Store `deviceToken` and `serialNumber` in NVS.
- Manage the screen/menu if the module has a local UI (in a normal LINK configuration, the MCU does this).

### Typical confusion case

> "Why are there no `events` in MQTT for my errors?"

Because when `UartBridge` receives `Log` (0x60), it only calls the registered `logHandler_`. If the application code did not call `setLogHandler(...)` and then call `MqttClient::publishEvent(...)` inside it, nothing will go to `events`. This is **not a library bug** - it is firmware responsibility.

The reference iDryer Link firmware does this in `IdryerDevice::handleLog`. Your firmware should do the same if you need publication into `events`.

## Separation between "iDryer Link product" and the `idryer-protocol` library

| Artifact | What it is | Where it lives |
|----------|---------|-----------|
| **iDryer Link** | Reference consumer firmware for the ESP32 module (with screen, menu, and staging scripts) | Separate repository [idryer-link](https://github.com/pavluchenkor/idryer-link) |
| **idryer-protocol** | The library Link is built on. Available as `lib_deps` in PlatformIO | This repository |

If you are **building your own device**, `idryer-protocol` is enough. You can look at Link as a live example of library usage.

## What next

- [04-glossary.md](04-glossary.md) - full glossary of terms.
- [../02-getting-started/01-choose-your-path.md](../02-getting-started/01-choose-your-path.md) - choose your path.
