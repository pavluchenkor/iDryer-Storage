# WebSocket Local Access: Control Without Cloud

An alternative device-control channel **on the local network without cloud**. Useful when:

- the internet is temporarily unavailable;
- the device is on an isolated local network;
- the user wants local control for security reasons.

A full MQTT API equivalent: the same JSON commands, the same action list. Only the transport changes - WebSocket instead of MQTT.

!!! note "UART Codes and Structure Details"
    `0x73`-`0x76`. The `WsEnablePayload` and `WsStatusPayload` structures are described in [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

---

## Architecture

```
   RP2040 (MCU)                            ESP32 (LINK)
  ┌─────────────────┐                     ┌────────────────────────────────┐
  │ Menu → WsEnable │──UART 0x73─────────►│ WsServer (port 81)             │
  │                 │                     │  ├─ mDNS (_idryer._tcp)         │
  │ Screen ← WsStatus│◄─UART 0x74─────────┤  ├─ WebSocketsServer            │
  │                 │                     │  ├─ PIN auth (4 digits)         │
  │ Menu → WsReset  │──UART 0x75─────────►│  └─ Client binding (max 5, NVS) │
  │ Menu → WsReq    │──UART 0x76─────────►│                                 │
  └─────────────────┘                     │ CloudStateMachine + MQTT         │
                                           └────────────────────────────────┘
                                                   │           │
                                              WS :81      MQTT :8883
                                                   │           │
                                               ┌───┴───────────┴───┐
                                               │    App / Client   │
                                               └───────────────────┘
```

WS and MQTT can work **at the same time**. A command that arrives over WS is applied the same way as an MQTT command.

---

## UART Contract

| Code | Name | Direction | Payload | Description |
|------|------|-----------|---------|-------------|
| `0x73` | `WsEnable` | MCU → LINK | `WsEnablePayload` (4 bytes) | Enable or disable the server, pass the PIN |
| `0x74` | `WsStatus` | LINK → MCU | `WsStatusPayload` (6 bytes) | Status for display on the screen |
| `0x75` | `WsResetClients` | MCU → LINK | — (empty) | Reset all client bindings |
| `0x76` | `WsStatusRequest` | MCU → LINK | — (empty) | Request the current status |

### Typical Scenarios

**Enable WS:**

```
MCU → WsEnable { enable=1, pin=4829 }
LINK: starts WsServer + mDNS
LINK → WsStatus { state=Listening, pin=4829, paired=0, max=5 }
MCU: shows the PIN in the menu as read-only
```

**Client connection:**

```
App → WS connect ws://DEVICE_<serial>.local:81
App → {"type":"auth","pin":"4829","clientId":"app-uuid-xxx"}
LINK → {"type":"auth_ok","deviceName":"DEVICE_<serial>"}
LINK → WsStatus { state=Connected, pin=4829, paired=1, max=5 }
```

**Reset bindings:**

```
MCU → WsResetClients
LINK: clears NVS client bindings, disconnects clients
LINK → WsStatus { state=Listening, pin=4829, paired=0, max=5 }
```

**Disable:**

```
MCU → WsEnable { enable=0, pin=0 }
LINK: stops WsServer and mDNS
LINK → WsStatus { state=Disabled, pin=0, paired=0, max=5 }
```

---

## WebSocket Protocol (ESP32 ↔ App)

### General

- Port: **81**
- Protocol: WebSocket (RFC 6455), text frames
- Message format: JSON
- mDNS: `DEVICE_<MAC>_<random>.local` (the name matches `serialNumber`)
- mDNS service: `_idryer._tcp`, port 81
- Maximum of 5 bound clients

### PIN

- 4 digits (1000-9999).
- Generated **on the MCU** when WS is enabled for the first time.
- Stored in MCU EEPROM (`ws_pin` field in the menu).
- Passed to LINK through `WsEnablePayload.pin`.
- LINK neither stores nor generates the PIN - it transparently uses the value from the MCU.

### Authorization

**First connection** (PIN required):

```json
→  {"type":"auth","pin":"4829","clientId":"app-uuid-xxx"}
←  {"type":"auth_ok","deviceName":"DEVICE_<serial>"}
```

On success, LINK stores `clientId` in NVS (the list of bound clients).

**Reconnect** (PIN not needed - the client is already bound):

```json
→  {"type":"auth","pin":"","clientId":"app-uuid-xxx"}
←  {"type":"auth_ok","deviceName":"DEVICE_<serial>"}
```

The server compares `clientId` with the list; if it is bound, it authorizes without a PIN.

**Invalid PIN:**

```json
→  {"type":"auth","pin":"9999","clientId":"..."}
←  {"type":"auth_failed","reason":"invalid_pin"}
```

The client disconnects.

---

## Commands and Publications

After authorization, the WS client receives the same command set as MQTT. Format:

```json
→  {"type":"cmd","command":"drying","data":{"unitId":"U1","params":{"temperature":55,"duration":240}}}
```

The server sends `data` to the same `CommandHandler::handleMqttCommand("drying", data)` as MQTT. There is no difference in how it is applied.

Publications from the device (topic equivalent):

```json
←  {"type":"telemetry","data":{ ... }}
←  {"type":"status","data":{ ... }}
←  {"type":"events","data":{ ... }}
```

The `data` formats match the MQTT publications - see [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md).

---

## Firmware Lifecycle

1. MCU enables WS from the menu → UART `WsEnable{enable=1, pin=...}`.
2. LINK starts `WsServer` and registers mDNS.
3. LINK sends `WsStatus{Listening}` → MCU displays the PIN.
4. The client connects through mDNS or IP:81 and completes auth.
5. `WsStatus{Connected}` → MCU shows "connected".
6. Commands and publications flow both ways.
7. If the connection drops, the server returns to `Listening`.

---

## WS States on the LINK Side

| Value | Name | Description |
|-------|------|-------------|
| 0 | `Disabled` | WS is disabled |
| 1 | `Listening` | Started, waiting for a connection |
| 2 | `Connected` | Client is connected and authorized |

---

## When to Use WS Instead of MQTT

- **No internet**, but local WiFi is available.
- **Privacy:** data does not leave the cloud.
- **Lower latency:** local channel is faster than a double hop through the broker.
- **Offline deployment:** factory testing, demo without cloud setup.

When MQTT + WS run at the same time, commands are applied the same way and publications go through both channels (both MQTT and WS). The app can see its data locally and through the cloud.

---

## What's Next

- [01-rfid.md](01-rfid.md) - RFID functionality.
- [../04-mqtt/](../04-mqtt/) - the same logic, but through the cloud.
- [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md) - `WsEnablePayload` / `WsStatusPayload`.
