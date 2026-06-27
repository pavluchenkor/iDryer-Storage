# Storage Link — product docs

This repository no longer documents the old RJ45/UART Link product.
The current product is `Storage Link`: ESP32 firmware that controls an addressable LED strip and optionally publishes climate data from an SHT31 sensor.

Current product responsibilities:

- highlight a requested LED position so an app can point to the needed spool or slot;
- publish `temperature` and `humidity` when an SHT31 sensor is present.

Russian product docs are the primary source of truth:

- [docs/ru/README.md](../ru/README.md)

Key sections:

- [guide](../ru/guide/README.md)
- [LED control](../ru/features/led-control.md)
- [Climate sensor](../ru/features/climate-sensor.md)
- [MQTT commands](../ru/protocol/mqtt-commands.md)
- [Local WebSocket access](../ru/protocol/local-access.md)
- [Hardware and build](../ru/reference/hardware-and-build.md)
