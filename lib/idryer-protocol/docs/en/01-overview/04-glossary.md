# Glossary of Terms and Identifiers

Unified vocabulary for the entire documentation set. If you encounter an unfamiliar term anywhere in the docs, look it up here first.

---

## System Nodes

### MCU

**M**icro**C**ontroller **U**nit. The dryer controller is a separate board with a microcontroller that controls the hardware (heater, sensors, display, EEPROM). In the reference firmware, this is RP2040, but the library is not tied to any specific chip. Synonyms: **Controller**, **RP2040** (in the context of the reference firmware).

### LINK

An ESP32-based network module. It provides the device with WiFi, MQTT, and an HTTP client. It is implemented as firmware built on top of the `idryer-protocol` library. Synonyms: **Network module**, **ESP32** (in the context of the reference firmware), **bridge** (when emphasizing its UART-to-MQTT bridge role).

### Cloud / Portal / Backend

iDryer server infrastructure. It includes the REST API, MQTT broker, device and user database, and the web and mobile applications. Public entry point: [portal.idryer.org](https://portal.idryer.org).

- **Portal** means the REST API or web UI.
- **Backend** means the server-side code.
- **Cloud** is the general umbrella term.

### Unit

One drying chamber. It has its own heater, sensors, and operating mode. A device can have 1 to 4 units.

- In UART, it is indexed as `unitId: 0, 1, 2, 3`.
- In MQTT, it is sent as a string: `U1, U2, U3, U4`.
- The `0xFF` value in UART means "all units" (for the `stop` command).

---

## Identifiers

### serialNumber

The unique identifier of the **LINK module**. It is used as:

- the prefix of all MQTT topics: `idryer/<serialNumber>/...`,
- the `Client ID` when connecting to the broker,
- the MQTT authorization `username`,
- the value in the `POST /devices/provision` request body.

**Format** (portal validation):

- `DEVICE_<mac>` or `DEVICE_<mac>_<suffix>`: the typical form for an ESP32-based module derived from the WiFi MAC address,
- or exactly 16 hex characters (for standalone devices where a MAC-based form is not suitable, for example when a single MCU does everything).

**Examples:**

```
DEVICE_aabbccddeeff
DEVICE_aabbccddeeff_1234567
36B955AB4350FEDC
```

**Source:** LINK firmware generates it on first boot and stores it in NVS. On a deployed device, it remains stable for the lifetime of the device.

### mcuSerial

The unique identifier of the **controller** (MCU). It is sent inside `HelloPayload.mcuSerial` (17 bytes: 16 hex characters + `\0`). For RP2040, this is the chip flash ID in hex form.

**Do not confuse it with `serialNumber`:**

- `serialNumber` belongs to LINK and is used for the cloud.
- `mcuSerial` belongs to the MCU and is used for diagnostics.

LINK forwards `mcuSerial` into the MQTT `info` topic as a separate JSON field. The portal **does not use** it to identify the device; it is reference data only.

### deviceToken

The secret MQTT authorization token for LINK. It is issued by the portal in response to `POST /devices/provision`.

**Lifecycle:**

- LINK stores it in NVS.
- It is used as the MQTT broker **password**.
- The broker validates the `(serialNumber, deviceToken)` pair through the backend HTTP auth hook.

**Do not confuse it with a user JWT:**

- `deviceToken` is issued to the device without authentication and is used for device MQTT access.
- A user JWT is issued to the user after login in the app and is used for `POST /devices/claim`.

If the device is already claimed (`isClaimed: true`), a repeated provision request returns `deviceToken: null`. This protects against device impersonation. In that case, the device must guide the user through a recovery flow (remove the device in the app, then claim it again).

### deviceId

The UUID of the device in the portal database. It is created after a successful claim.

**Used by:**

- LINK receives it in the `GET /devices/check-claim/:token` response after a successful claim.
- MCU receives it in `ClaimCompletePayload.deviceId`, **for display on the screen only**.
- The app uses it when working with the portal API.

It is **not used** in MQTT topics or MQTT authorization. For MQTT, `serialNumber` + `deviceToken` is sufficient.

### User JWT

A JSON Web Token received by the user after login in the portal app. It contains the user ID, expiration time, and signature. It is used as `Authorization: Bearer <jwt>` when the app calls `POST /devices/claim`.

**The device (LINK) does not need a JWT.** The device relies on `deviceToken`.

### PIN (claiming)

An eight-digit code that the portal returns to the device in response to `POST /devices/register`. TTL is 10 minutes. The user sees the PIN on the dryer screen and enters it in the app to claim the device to their account.

A separate **WebSocket PIN** (4 digits, 0-9999) is a different entity. Do not confuse them. See [../07-features/02-websocket-local.md](../07-features/02-websocket-local.md).

---

## Protocols and Formats

### UART frame (Frame)

The unit of exchange over UART. It consists of a header (`FrameHeader`, 6 bytes), a payload (0-200 bytes), and two CRC bytes. It starts with SOF = `0xAA`. Details: [../03-uart/02-frame-and-crc.md](../03-uart/02-frame-and-crc.md).

### MessageKind

The UART frame type (`enum uint8_t`). It defines how to interpret the payload. Examples: `Hello = 0x01`, `Telemetry = 0x10`, `Command = 0x20`. Full list: [../03-uart/03-message-types.md](../03-uart/03-message-types.md).

### Payload

The useful data carried by a UART frame. The structure depends on `MessageKind`. All payload structures are packed with `#pragma pack(1)`, and all multi-byte values are little-endian. Details: [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

### CRC16-CCITT

The UART frame checksum. Polynomial `0x1021`, initial value `0xFFFF`, result written to the frame in little-endian order (least significant byte first). Reference implementation: `src/uart/uart_protocol.cpp`.

### SEQ (sequence)

The frame sequence number (`uint8_t`, 0-255, incremented for every transmitted frame). In an `ACK` frame, `SEQ` equals the sequence number of the frame being acknowledged.

### ACK / ACK_REQUIRED

The delivery acknowledgment mechanism. The sender sets the `ACK_REQUIRED = 0x01` flag in the header. The receiver replies with an ACK frame (`TelemetryAck = 0x11`, `CommandAck = 0x21`, `ConfigAck = 0x31`) with the corresponding `SEQ`. Timeout is 700 ms, with up to 3 retries. Details: [../03-uart/05-ack-retry.md](../03-uart/05-ack-retry.md).

### Heartbeat

A periodic "I am alive" frame. Both sides send it every 5 seconds. It carries uptime, RSSI (or MCU temperature), an error counter, and the current cloud state. Structure: `HeartbeatPayload`.

### Provision / Register / Claim / Check-claim

Stages of device claiming:

- **Provision**: LINK receives `deviceToken`.
- **Register**: LINK receives a PIN and shows it to the user.
- **Claim**: the user enters the PIN in the app, and the app calls `POST /devices/claim`.
- **Check-claim**: LINK polls the portal to find out whether the user has completed the claim.

Details: [../05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md).

---

## MCU Operating Modes

The `DryerMode` enum in UART:

| Value | Name | Description |
|----------|-----|----------|
| `0` | `Idle` | The device is idle |
| `1` | `Drying` | Regular drying (temperature + duration) |
| `2` | `Storage` | Storage mode (temperature + humidity, no time) |
| `3` | `Profile` | Multi-stage profile |
| `4` | `Fault` | Error |

In MQTT, these values are sent as strings: `"IDLE"`, `"DRYING"`, `"STORAGE"`, `"PROFILE"`, `"FAULT"`.

A separate `DryerState` enum (Idle/Preheat/Drying/Cooling/Fault/Service) is a more detailed internal MCU state. It is declared in the library, but the reference protocol layer does not use it: it sends `DryerMode`, not `DryerState`.

---

## Cloud Connection States

The `LinkCloudState` enum in `HeartbeatPayload.cloudState`:

| Value | Name | Description |
|----------|-----|----------|
| `0` | `Idle` | Initial state before `begin()` |
| `1` | `WifiConnecting` | Connecting to WiFi |
| `2` | `Provisioning` | Running `POST /provision` |
| `3` | `Registering` | Running `POST /register` |
| `4` | `AwaitingClaim` | Waiting for the user to enter the PIN |
| `5` | `Ready` | WiFi + token + deviceId received |
| `6` | `MqttConnecting` | Connecting to MQTT |
| `7` | `Online` | Everything is working |

The MCU uses these values for status indication (LED/display).

---

## MQTT Terms

### Topic prefix

All device topics start with `idryer/<serialNumber>/`. Example: `idryer/DEVICE_aabb.../telemetry`.

### LWT (Last Will and Testament)

An MQTT mechanism: if the connection drops unexpectedly (without `DISCONNECT`), the broker publishes a predefined message on the client's behalf. LINK configures LWT for the `idryer/<serial>/offline` topic with body `{}`. This lets the server know that the device went offline due to a network failure rather than a graceful shutdown.

### QoS

The MQTT delivery guarantee level: 0 (fire-and-forget), 1 (at-least-once), 2 (exactly-once). iDryer uses QoS 0 for telemetry (loss is acceptable) and QoS 1 for everything else.

### Retained

An MQTT flag meaning "store the last message and deliver it to new subscribers." In iDryer, it is used for `info`, `status`, and `rfid`, so a new client (web UI or app) immediately sees the latest data without waiting for the next publication.

---

## Units and Formats

### Temperature

- In UART: `int16`, value x 10 (`553` means `55.3 °C`). This allows fractional values without using `float`.
- In MQTT (JSON): `float` with one decimal place (`55.3`).

### Humidity

- In UART: `uint16`, value x 10 (`452` means `45.2%`).
- In MQTT: `float` with one decimal place.

### Weight

- In UART: `uint16`, value x 10 grams (`1234` means `123.4 g`). Range up to 5000 (500 g).
- In MQTT: `float` in grams.

### Time

- UART uses seconds (`uint32`), JSON commands use seconds or minutes depending on the field.
- Timestamps in JSON use ISO 8601 in UTC (for example, `"2026-04-01T12:00:00Z"`).

### Firmware version

Packed into `uint32`: `(MAJOR << 16) | (MINOR << 8) | PATCH`. For example, version `1.2.3` = `0x00010203`.

---

## MQTT Terms in 5 Minutes

For those who have published telemetry to ThingSpeak but have not gone deeper.

### QoS (Quality of Service)

The delivery guarantee level for a single message:

- **QoS 0** means "at most once", fire-and-forget. Send it and forget it. If it is lost, nobody will know. Fastest mode. Used for `telemetry` (the next sample arrives in 5 seconds anyway).
- **QoS 1** means "at least once". The sender retries until it receives PUBACK from the receiver. The message is guaranteed to arrive, but **it may arrive twice**. Used for `status`, `info`, and `commands/*`.
- QoS 2 is not used in iDryer (more traffic, no meaningful benefit).

### Retained

A flag on a publish message. If `true`, the broker **stores** that message and **delivers it to every new subscriber** at subscribe time, without waiting for the next publication.

Example: the user's app connects to the portal, subscribes to `idryer/<serial>/info`, and immediately receives the retained message: "this is the device, firmware version 1.2.3, these are the chambers", even if the device is not publishing anything right now.

In iDryer, `retained: true` is used for `info`, `status`, `rfid`, and `integrations/status`: everything that represents the current state.

### Keepalive

The interval in seconds between client pings (default 60). If the client sends neither data nor `PINGREQ`, the broker considers it **dead** after the keepalive timeout, closes the session, and publishes the LWT if configured.

On the client side, PubSubClient sends `PINGREQ` roughly every 45 seconds. If WiFi is poor, the packet may be lost and the broker will terminate the session.

### LWT (Last Will and Testament)

The client's "will." When connecting, the client tells the broker: "if I disappear unexpectedly, publish **this** on my behalf." The broker publishes the configured message to the configured topic **only** on an abnormal disconnect (not on a normal `disconnect`).

iDryer uses LWT for the `idryer/<serial>/offline` topic with body `{}` so the backend can detect device failure immediately instead of waiting for the keepalive timeout.

### Clean session / Persistent session

A flag in the CONNECT packet. It determines whether the broker **remembers the client** across reconnects.

- **`clean_session = true`** means every CONNECT starts from scratch. Subscriptions are cleared, queued QoS 1 messages are lost. In the protocol, this is denoted as `c1`.
- **`clean_session = false`** means the broker keeps the subscription and QoS 1 queue for that `clientId`. On connection loss, commands are not lost. This is denoted as `c0`.

In iDryer, `clean_session = false` is always used. Otherwise, after each reconnect the client may fail to resubscribe in time (the SUBSCRIBE packet can be lost in WiFi jitter), and commands from the portal will stop arriving.

### Wildcards `+` and `#`

MQTT subscriptions can use wildcards:

- `+` means **one level**: `idryer/+/telemetry` matches `idryer/DEV_AAA/telemetry`, `idryer/DEV_BBB/telemetry`, but not `idryer/DEV_AAA/sub/telemetry`.
- `#` means **everything below**: `idryer/DEV_AAA/commands/#` matches `idryer/DEV_AAA/commands/drying`, `commands/stop`, and so on. Only at the **end** of the topic.

The device subscribes to `idryer/<serial>/commands/#` and receives all commands.

### TLS (Transport Layer Security)

Encryption plus authentication on top of TCP. In iDryer, MQTT runs over TLS on port 8883 (not plain 1883).

The client needs the **root certificate (CA)** of the certificate authority that signed the server certificate. In code, this is `WiFiClientSecure` + `setCACert(ROOT_CA_LETSENCRYPT)`. For Let's Encrypt, the CA is stored directly in the firmware.

For LAN connections (for example, a Bambu Lab printer with a self-signed certificate), `setInsecure()` is acceptable: the client does not validate the server certificate. This is acceptable on LAN, never on the public internet.

---

## Web Terms in 5 Minutes

### JWT (JSON Web Token)

A signed JSON token for HTTP authorization. Format: `header.payload.signature`, with dot separators (base64). In the HTTP header, it is sent as `Authorization: Bearer <jwt>`.

In iDryer, the **device** does not need a JWT (it authenticates with `deviceToken` over MQTT). JWT is needed only by the portal user: the app receives it at login and uses it for `POST /devices/claim`.

### REST API

HTTP + JSON by contract: methods `GET/POST/PUT/DELETE`, body in JSON. The iDryer portal provides REST endpoints `/devices/provision`, `/devices/register`, `/devices/claim`, `/devices/check-claim/:token`. ESP32 calls them through `HTTPClient` (with TLS in production).

### mDNS (multicast DNS)

A protocol for automatic device discovery on a local network. It lets you reach devices by names like `myhost.local` without configuring a DNS server. It works through multicast queries within a single subnet.

iDryer uses mDNS to discover Home Assistant (`homeassistant.local`) and, optionally, Klipper/Moonraker (`klipper.local`). On ESP32, it works through the `ESPmDNS` library.

### WebSocket

A bidirectional persistent channel on top of TCP (RFC 6455). Unlike HTTP request/response, WebSocket means "open a connection, then send packets in both directions as needed until the connection is closed."

Used for:

- The local WS server on LINK for the user's app without the cloud (section [../07-features/02-websocket-local.md](../07-features/02-websocket-local.md)).
- The Moonraker client (`ws://klipper.local:7125/websocket`) for real-time subscription to Klipper status.

### JSON-RPC 2.0

The format for remote calls via JSON messages:

```json
// Request
{ "jsonrpc": "2.0", "method": "printer.objects.query", "params": {...}, "id": 1 }
// Response (success)
{ "jsonrpc": "2.0", "result": {...}, "id": 1 }
// Notification (no id, server -> client, no response)
{ "jsonrpc": "2.0", "method": "notify_status_update", "params": [...] }
```

Over WebSocket, this is the Moonraker API. The `id` field matches a response to a request; notifications do not have an `id`.

### SSE (Server-Sent Events)

A one-way "server to client" channel over HTTP. The server keeps a long-lived response open and sends event after event. Unlike WebSocket, the client cannot send data back to the server over the same connection.

iDryer does not use SSE on the device side, but the portal may use SSE for push status updates in the browser UI.

### NVS (Non-Volatile Storage)

ESP32 non-volatile memory. Similar to EEPROM. A key-value store split into namespaces. Accessed through the `Preferences.h` API. In iDryer, it stores `serialNumber`, `deviceToken`, WiFi credentials, and LINK integration settings.

A namespace is a "folder" for key-value pairs. Namespace name length is <=15 characters. In iDryer:

- `idryer` stores the main credentials.
- `li_ha`, `li_bambu`, `li_moon`, `li_common` store LINK integration settings.
- `ha` stores legacy Home Assistant settings (through mDNS).

### ACL (Access Control List)

MQTT broker rules that define who can publish or subscribe to which topic. In Mosquitto, they are configured through the `acl_file`. In iDryer, device `<serial>` is allowed to publish and subscribe only within `idryer/<serial>/...`.

---

## Abbreviations

| Abbreviation | Meaning |
|------------|-------------|
| MCU | MicroController Unit - controller |
| LINK | ESP32 module, UART-to-cloud bridge |
| LE | Little-Endian - byte order, least significant byte first |
| SOF | Start Of Frame - the start byte of a UART frame (`0xAA`) |
| CRC | Cyclic Redundancy Check - checksum |
| ACK | Acknowledgement - confirmation |
| NVS | Non-Volatile Storage - ESP32 non-volatile memory |
| LWT | Last Will and Testament - the MQTT client's "will" |
| RSSI | Received Signal Strength Indicator - WiFi signal strength |
| TTL | Time To Live - expiration period |
| PIN | Personal Identification Number - code for claim or WS |
| JWT | JSON Web Token - portal user token |
| QoS | Quality of Service - MQTT guarantee level |
| UUID | Universally Unique Identifier - `deviceId` |
| EEPROM | Persistent MCU memory for user settings |
| WS | WebSocket - local channel without the cloud |

---

<- [Back to the 01-overview table of contents](../README.md)
