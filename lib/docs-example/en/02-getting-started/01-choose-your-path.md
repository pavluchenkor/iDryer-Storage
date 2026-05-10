# Choose Your Path

The library and the documentation cover three common tasks. Choose yours and you get a quickstart that takes you from "I have a clean IDE" to "I can see my telemetry in the iDryer portal".

!!! note "Before You Start"
    Read [../01-overview/](../01-overview/). Without a shared picture of the three nodes (MCU/LINK/Cloud), the quickstart is harder to follow.

## Path 1: "Your Own Controller" (MCU)

**You do:**

- Build your own board with a microcontroller (STM32, RP2040, AVR, any Arduino-compatible board).
- Write your own firmware for that board: sensors, PID, screen, menu.
- Use the **ready-made** LINK module (the reference iDryer ESP32 module) to reach the cloud.

**You use from the library:** only the UART section - frame structures and CRC16. Everything is header-only, with no runtime.

**Quickstart:** [02-quickstart-controller.md](02-quickstart-controller.md).

The quickstart shows how to build a `Hello` frame, calculate CRC, send it over UART, and receive `HelloAck` **without PlatformIO and without dependencies**. After that you understand the protocol from the inside and can move the code to any platform.

## Path 2: "Your Own Network Module" (LINK)

**You do:**

- Build your own ESP32 board (or a compatible WiFi module).
- Write your own firmware: cloud connectivity, UART ↔ MQTT bridge.
- Work with a third-party MCU (someone else's dryer or your own).

**You use from the library:** everything - UART, MQTT, HTTP, command parser, cloud state machine.

**Quickstart:** [03-quickstart-bridge.md](03-quickstart-bridge.md).

The quickstart takes you through the PlatformIO build, adding `idryer-protocol` as `lib_deps`, the first run with Hello validation from the MCU, and the first MQTT traffic.

## Path 3: "One ESP32 for Everything" (Standalone)

**You do:**

- Build a device on one ESP32, where the same chip controls hardware and talks to the cloud.
- There is no separate MCU and no UART between boards.
- Typical cases: iHeater, telemetry module, simple products.

**You use from the library:** MQTT, HTTP, command parser, state machine. You do not need the UART section - `CommandPayload` and `ProfilePayload` are used directly in code through your own `ICommandSink` implementation.

**Quickstart:** [04-quickstart-standalone.md](04-quickstart-standalone.md).

## What to Use Matrix

| Library component | MCU | LINK | Standalone |
|-------------------|-----|------|------------|
| UART: payload structures (`*.h`) | ✅ main | ✅ | ⚠️ types only (`CommandPayload`, etc.) |
| UART: `UartBridge` (runtime) | optional | ✅ main | ❌ not needed |
| UART: `calculateCrc()` | ✅ | ✅ | ❌ |
| MQTT: `MqttClient` | ❌ | ✅ | ✅ |
| HTTP: `HttpApi` | ❌ | ✅ | ✅ |
| `CommandHandler` | ❌ | ✅ | ✅ |
| `ICommandSink` - `UartCommandSink` | - | ✅ ready-made | ❌ you write your own |
| `CloudStateMachine` | ❌ | ✅ | ✅ |
| `TelemetryPublisher` | ❌ | ✅ | ✅ |

## If You Are Not Sure

1. Do you have a separate board with WiFi? If yes, you need LINK and your main code lives in the MCU - **path 1**.
2. Are you planning to build your own WiFi module? - **path 2**.
3. Do you want to fit everything into one ESP32? - **path 3**.

## After the Quickstart Works: Claim to an Account

For the device to appear in the user's app and accept commands from the portal, you need to **claim it to an account**.

There are three ways to claim it:

- **Through the device screen** - the standard way for iDryer devices with UI.
- **Through the web installer** - the standard way for headless devices.
- **Through Serial Monitor** - a dev test when there is neither a screen nor an installer for your device type, which is relevant for developers of new products.

All three are described in [05-dev-claim.md](05-dev-claim.md).

## After the Quickstart

The quickstart gives you the minimal working skeleton. Continue in order:

- [../03-uart/](../03-uart/) - the full UART protocol in detail.
- [../04-mqtt/](../04-mqtt/) - the MQTT contract.
- [../05-cloud/](../05-cloud/) - HTTP and device claiming.
- [../06-flows/](../06-flows/) - end-to-end flows (first boot, claiming, remote config, profile).
- [../07-features/](../07-features/) - optional: RFID, local WebSocket.
