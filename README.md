# iDryer Storage — Firmware

ESP32 firmware for Storage Link — an LED strip controller with climate monitoring. Connects to iDryer cloud via MQTT.

## Active Product

**Storage Link** (`src/storage/`) — controls addressable LED strips, highlights the requested LED position for a spool/slot, reads an optional SHT31 climate sensor, and connects to [portal.idryer.org](https://portal.idryer.org/) over MQTT.

## Library

`lib/idryer-core` — shared platform layer: WiFi provisioning, cloud state machine, MQTT client, NVS credential store, HAL interfaces.

## Entry Point

`src/main.cpp` — Arduino `setup()`/`loop()`, Improv WiFi, LED strip init, telemetry loop.  
Product-specific code: `src/storage/` — `LedStripExecutor`, `LedStripProfile`, `StorageTelemetryPublisher`.

---

## Build

```bash
pio run                              # build all default_envs (production variants)
pio run -e <env>                     # build specific env
pio run -e <env> -t upload           # flash to device
pio device monitor                   # serial monitor (115200 baud)
```

---

## Supported Boards

| Env name | Board | Notes |
|---|---|---|
| `esp32c3-storage-prod` | ESP32-C3 DevKitM-1 | Production, TLS MQTT |
| `esp32c3-storage-stage` | ESP32-C3 DevKitM-1 | Staging, no TLS |
| `esp32c3-super-mini-prod` | ESP32-C3 Super Mini | Production, TLS MQTT |
| `esp32c3-super-mini-stage` | ESP32-C3 Super Mini | Staging, no TLS |
| `xiao-esp32s3-prod` | Seeed XIAO ESP32-S3 | Production, TLS MQTT |
| `xiao-esp32s3-stage` | Seeed XIAO ESP32-S3 | Staging, no TLS |
| `waveshare-esp32s3-zero-prod` | Waveshare ESP32-S3 Zero | Production, TLS MQTT |
| `waveshare-esp32s3-zero-stage` | Waveshare ESP32-S3 Zero | Staging, no TLS |

`default_envs` = `esp32c3-storage-prod`, `esp32c3-super-mini-prod`, `xiao-esp32s3-prod`, `waveshare-esp32s3-zero-prod`

---

## Environments

**Production** (`-prod`):
- MQTT broker: `mqtt.idryer.org:8883`, TLS enabled
- API: `https://portal.idryer.org/api`
- `CORE_DEBUG_LEVEL=0`
- Post-build: copies firmware artifacts to `firmware/<board-name>/`

**Staging** (`-stage`):
- MQTT broker: `staging.idryer.org:1884`, no TLS
- API: `https://staging.idryer.org/api`
- `CORE_DEBUG_LEVEL=3`
- Post-build: runs `stage_auto_claim.py`

---

## Documentation

Russian docs: [`docs/ru/`](docs/ru/README.md)

| Section | Content |
|---|---|
| [docs/ru/guide/README.md](docs/ru/guide/README.md) | Product overview and runtime behavior |
| [docs/ru/features/led-control.md](docs/ru/features/led-control.md) | LED highlighting for the requested spool/slot |
| [docs/ru/features/climate-sensor.md](docs/ru/features/climate-sensor.md) | Optional SHT31 temperature/humidity telemetry |
| [docs/ru/protocol/mqtt-commands.md](docs/ru/protocol/mqtt-commands.md) | MQTT commands and published topics |
| [docs/ru/protocol/local-access.md](docs/ru/protocol/local-access.md) | LAN WebSocket API |
| [docs/ru/reference/hardware-and-build.md](docs/ru/reference/hardware-and-build.md) | Boards, pins, envs, build flow |
| [docs/ru/reference/config-menu.md](docs/ru/reference/config-menu.md) | Config menu IDs, ranges, defaults |
| [docs/ru/portal/device-api.md](docs/ru/portal/device-api.md) | What portal/app should do for this product |

---

## Firmware Binaries

Pre-built artifacts per production env:

```
firmware/
├── esp32c3-storage/          # esp32c3-storage-prod
├── esp32c3-super-mini/       # esp32c3-super-mini-prod
├── xiao-esp32s3/             # xiao-esp32s3-prod
└── waveshare-esp32s3-zero/   # waveshare-esp32s3-zero-prod
```

Each directory contains: `firmware.bin`, `bootloader.bin`, `partitions.bin`, `boot_app0.bin`.

---

## Reference Code

`reference/` — migration and reference code only. Not compiled by default. Contains previous iHeater and Dryer Link implementations.
