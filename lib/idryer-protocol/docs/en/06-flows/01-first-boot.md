# First boot: from power-on to Online

Scenario: the device is powered on for the first time after factory assembly. It needs to reach the state “working, visible in the user app, telemetry is flowing.” In chronological order.

!!! note
    This is an end-to-end flow. Individual details are in [../03-uart/](../03-uart/), [../04-mqtt/](../04-mqtt/), [../05-cloud/](../05-cloud/).

---

## Participants

- **MCU** — your controller.
- **LINK** — the network module.
- **Cloud** — portal (REST + MQTT).
- **User** — with the app.

---

## Stages

### 0. Boot

Both devices power on. Boot times:

- MCU — usually 100-500 ms.
- LINK (ESP32) — 1-3 seconds until the WiFi stack is ready.

Which one becomes ready first depends on the hardware. The protocol is symmetric, so this does not matter.

### 1. UART handshake

**Scenario A (MCU ready first):**

```
MCU  ──Hello(role=0x01, units, fw, mcuSerial)──►  LINK
     (LINK may still not be ready — ACK will not come back; MCU repeats Hello every 5 s)
MCU  ──Hello────────────────────────────────────►  LINK  (ready)
MCU  ◄──HelloAck(ip=0, ssid="")──────────────────  LINK  (WiFi is not connected yet)
```

**Scenario B (LINK ready first):**

```
LINK ──Hello(role=0xFF, HelloRequest)────────────►  MCU
MCU  ──Hello(role=0x01, units, fw, mcuSerial)───►  LINK
LINK ──HelloAck──────────────────────────────────►  MCU
```

Details are in [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md), section “Who speaks first”.

At this stage, LINK knows the MCU firmware version, `deviceType`, chamber topology, and `mcuSerial`. This information is needed to publish `info` over MQTT.

### 2. WiFi connect (LINK)

LINK reads WiFi credentials (SSID/password) from NVS and connects. If they are missing, it starts AP mode with a captive portal (in the reference Link firmware this is implemented through `ArduinoWifiManager`; on the device side it is separate UX and is outside the scope of the library).

While WiFi is not ready, the `cloudState` field in Heartbeat LINK→MCU is `WifiConnecting (1)`.

When WiFi comes up, LINK gets an IP address and optionally sends a repeated `HelloAck` with populated `ipAddress` + `ssid` if the application code does that; this is not required.

### 3. Provision and register (claiming)

If `deviceToken` is still missing in NVS, the device is **unclaimed**. It must go through the full claiming flow.

The user presses “start claiming” in the MCU menu. Continue with [02-claiming-flow.md](02-claiming-flow.md).

If the device is already claimed (the token is in NVS), skip this step and go to step 4.

**cloudState during this phase:** `Provisioning (2)` → `Registering (3)` → `AwaitingClaim (4)` → `Ready (5)`.

### 4. MQTT connect

LINK knows its `serialNumber` and `deviceToken`. It connects to the MQTT broker.

```
LINK ──TLS handshake────────────────────►  Broker
LINK ──MQTT CONNECT(clientId=serialNumber,
                   username=serialNumber,
                   password=deviceToken,
                   LWT=idryer/<serial>/offline)──►  Broker
LINK ──SUBSCRIBE idryer/<serial>/commands/#──────►  Broker
```

**cloudState:** `MqttConnecting (6)` → `Online (7)`.

See [../04-mqtt/01-connection.md](../04-mqtt/01-connection.md) for details.

### 5. Publish `info` (retained)

After `Online`, LINK publishes `info` once:

```json
{
  "hardwareVersion": "v1.0",
  "firmwareVersion": "1.2.3",
  "workTimeCounter": 360000,
  "unitsCount": 1,
  "mcuSerial": "36B955AB4350FEDC",
  "deviceType": "dryer",
  "units": [ { "unitId": 0, "capabilities": {...}, "scales": [], "rfid": [0] } ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

Retained means that if the user app subscribes to `idryer/<serial>/info`, it will **immediately** receive the latest device data, even if the device is not online at that moment.

Backend portal: when it receives `info` from `Link` in the `CLAIMED` state, it moves the record to `BOUND`.

### 6. Normal operation

After that, normal operation continues:

| Period | Action |
|--------|--------|
| every 1 s (active) / 15 s (idle) | MCU → UART Telemetry → LINK → MQTT telemetry |
| on mode change | MCU → UART Status → LINK → MQTT status |
| every 5 s | Heartbeat in both directions (UART) |
| every 10 s / on change | MCU → UART Weights → LINK → MQTT weights |
| on `tag_detected` / `tag_removed` event | MCU → UART Rfid → LINK → MQTT rfid |
| on app event | MCU → UART Log → LINK → MQTT events |

Plus:

- LINK listens to MQTT `commands/#` and, on an incoming command, turns it into a UART `Command` / `ConfigPush` frame and sends it to the MCU.
- Every second, LINK services `UartBridge.loop()` and `MqttClient.loop()` in its main loop.

---

## Timeline in seconds (example)

```
t=0.0    Power on
t=0.3    MCU ready, sends Hello
t=1.5    LINK ready, WiFi begin
t=1.6    LINK received Hello → HelloAck
t=5.0    WiFi connected, cloudState = Ready
t=5.1    (If claimed) MQTT connect
t=5.8    MQTT Online, publish info
t=6.0    First telemetry published
...
t=5s production mode
```

For an unclaimed device, the process is extended by the claiming time — it can take from a few seconds to several minutes depending on the user.

---

## What to verify at the end

- [ ] `Heartbeat.cloudState == 7 (Online)` reaches the MCU every 5 seconds.
- [ ] The user app shows the device, and retained `info` has been loaded.
- [ ] `telemetry` is published with real data.
- [ ] MQTT `commands/drying` changes `status` to DRYING.
- [ ] When device power is turned off, an LWT appears in MQTT at `idryer/<serial>/offline`.

---

## Next steps

- [02-claiming-flow.md](02-claiming-flow.md) — detailed claiming flow.
- [03-remote-config.md](03-remote-config.md) — menu config exchange.
- [04-profile-mode.md](04-profile-mode.md) — profile drying.
