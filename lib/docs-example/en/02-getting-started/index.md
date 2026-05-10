# Quickstart

Five documents. The first is about choosing the path, meaning what you are actually doing. Three quickstarts cover the three paths. The fifth explains how to claim the device to an account, including development without flasher-portal.

| Document | For whom |
|----------|----------|
| [01-choose-your-path.md](01-choose-your-path.md) | Choose one of three paths |
| [02-quickstart-controller.md](02-quickstart-controller.md) | Your controller (MCU): manual UART frame assembly, CRC, hex dumps |
| [03-quickstart-bridge.md](03-quickstart-bridge.md) | Your network module (LINK) on ESP32 + PlatformIO |
| [04-quickstart-standalone.md](04-quickstart-standalone.md) | One ESP32 does everything: loop to Online, telemetry publishing |
| [05-dev-claim.md](05-dev-claim.md) | Three ways to claim the device: screen / flasher-portal / Serial Monitor (for dev testing) |

## Recommended Order

1. Read [01-choose-your-path.md](01-choose-your-path.md) to understand your scenario in 2 minutes.
2. Read the quickstart that matches your path: 02 / 03 / 04.
3. When the firmware builds and connects to WiFi, read [05-dev-claim.md](05-dev-claim.md) to see how to claim the device to an account.

## What Next

After the quickstart, continue deeper:

- [../03-uart/](../03-uart/index.md) - the full UART protocol in detail.
- [../04-mqtt/](../04-mqtt/index.md) - the MQTT contract.
- [../05-cloud/](../05-cloud/index.md) - the portal HTTP API.
- [../06-flows/](../06-flows/index.md) - end-to-end flows.
- [../07-features/](../07-features/index.md) - optional features (RFID, WebSocket, LINK integrations).
