# Optional Features

Everything that is **not required** for the base iDryer device, but adds value. Two native features (RFID tags, local WebSocket without cloud) and five documents about LINK integrations with external systems (Home Assistant, Bambu Lab, Moonraker/Klipper).

| Document | Feature |
|----------|---------|
| [01-rfid.md](01-rfid.md) | RFID readers: `tag_detected` / `tag_removed`, OpenPrintTag read/write |
| [02-websocket-local.md](02-websocket-local.md) | Local LINK WebSocket channel for offline operation |
| [03-link-integrations-overview.md](03-link-integrations-overview.md) | **Common contract** for the three LINK integrations: one `link_integration` topic, consolidated `integrations/status`, behavior by `deviceType` |
| [04-home-assistant.md](04-home-assistant.md) | Home Assistant: sensor publishing through mDNS + portal credentials |
| [05-bambu-integration.md](05-bambu-integration.md) | Bambu Lab LAN MQTT: **Writer** (RFID tag emulation for Dryer) / **Reader** (printer status for iHeater) |
| [06-moonraker-printer.md](06-moonraker-printer.md) | Moonraker (Klipper) WebSocket + **VIRTUAL_CHAMBER** macro for iHeater |
| [07-portal-integration-contract.md](07-portal-integration-contract.md) | **Contract for portal developers**: all three integrations in one document |

## How to Read

- **If you are a portal developer** → go straight to [07-portal-integration-contract.md](07-portal-integration-contract.md). It contains everything you need, with more detail below if you want it.
- **If you are developing LinkII firmware (iHeater)** → read [03-link-integrations-overview.md](03-link-integrations-overview.md) for the overall architecture, then [06-moonraker-printer.md](06-moonraker-printer.md) (this is the main source of the chamber target temperature).
- **If you are building an iDryer device with RFID** → read [01-rfid.md](01-rfid.md).
- **If you want local access without the internet** → read [02-websocket-local.md](02-websocket-local.md).

## Implementation Status

Most LINK integrations (Bambu / Moonraker / HA with credentials through the portal) are implemented in the library on `feat/link-integrations`→`dev`, see commits `Phase 1..4`. Details for each are in the corresponding documents.
