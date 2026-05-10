# MQTT

The LINK integration contract with the iDryer cloud: broker connection, topics, JSON formats for publications and commands, and examples for manual debugging.

| Document | Contents |
|----------|----------|
| [01-connection.md](01-connection.md) | Connection: TLS/CA, credentials, LWT, keepalive, **persistent session (clean_session=false)** |
| [02-topics.md](02-topics.md) | Complete topic table: device→backend and backend→device, QoS, retained |
| [03-device-to-backend.md](03-device-to-backend.md) | JSON formats for all device publications (info, telemetry, status, weights, rfid, events, config, integrations/status) |
| [04-backend-to-device.md](04-backend-to-device.md) | JSON formats for all portal commands (drying, stop, profile, set, invoke, link_integration, …) |
| [05-examples.md](05-examples.md) | Ready-to-use `mosquitto_pub`/`mosquitto_sub` commands for each command |

## Navigation

- Setting up your connection from scratch → [01-connection.md](01-connection.md).
- Seeing what the device publishes → [03-device-to-backend.md](03-device-to-backend.md).
- Seeing what the device accepts → [04-backend-to-device.md](04-backend-to-device.md).
- Debugging manually from the server → [05-examples.md](05-examples.md).

## Related

- [../03-uart/](../03-uart/index.md) — what arrives in LINK over UART, and what later becomes MQTT publications.
- [../05-cloud/](../05-cloud/index.md) — the HTTP part: how LINK gets `deviceToken` before connecting to MQTT.
- [../06-flows/03-remote-config.md](../06-flows/03-remote-config.md) — the end-to-end `set`/`invoke`/`config/delta` scenario.
