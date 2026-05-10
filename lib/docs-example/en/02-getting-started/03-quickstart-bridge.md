# Quickstart: Your Network Module (LINK)

The minimal path from an empty PlatformIO project to "the ESP32 bridge accepts Hello from the MCU over UART". Here you already use the whole library - no need to write the parser by hand when one is already available.

!!! note "Context"
    This quickstart goes **only as far as the first UART exchange** with the MCU. Cloud connectivity, MQTT, and claiming come in the next quickstart (standalone) and in [../04-mqtt/](../04-mqtt/), [../05-cloud/](../05-cloud/), [../06-flows/](../06-flows/).
    If you need the full UART + cloud chain, go through [02-quickstart-controller.md](02-quickstart-controller.md), this document, and [04-quickstart-standalone.md](04-quickstart-standalone.md) in order.

!!! warning "You Need the Basics"
    The code below actively uses `namespace` / `using namespace`, `const T&` references, `enum class`, callbacks (passing functions as parameters), and lambdas. If something is unclear, read [prerequisites](../01-overview/05-prerequisites.md) sections 5-9 first.

---

## Quickstart Goal

The ESP32 listens on UART, receives `Hello` from the MCU, replies with `HelloAck`, and prints all incoming telemetry to Serial Monitor. After that, WiFi and MQTT are the next topics.

---

## What You Need

**Hardware:**

- An ESP32 board of any kind: DevKit, NodeMCU-32, WROOM, S3, ...
- An MCU that already sends `Hello` and `Telemetry` (a ready-made iDryer controller or your board from [02-quickstart-controller.md](02-quickstart-controller.md)).

**Software:**

- VSCode with the **PlatformIO** extension, or the PIO CLI.
- Git, because the library is pulled in by URL.

**UART Wiring:**

```
   MCU                ESP32
  ┌───┐              ┌───┐
  │TX ├──────────────┤GPIO16 (RX1)│
  │RX ├──────────────┤GPIO17 (TX1)│
  │GND├──────────────┤GND│
  └───┘              └───┘
```

---

## Step 1. Create a PlatformIO Project

Create an empty project for the ESP32 board, then replace `platformio.ini` with:

```ini
[env:esp32dev]
platform    = espressif32
board       = esp32dev
framework   = arduino
monitor_speed = 115200

lib_deps =
  https://github.com/pavluchenkor/idryer-protocol.git
  bblanchon/ArduinoJson @ ^7.0.4
```

You will need `ArduinoJson` later, when publishing to MQTT.

---

## Step 2. `src/main.cpp`

Minimal bridge: receives `Hello`, replies with `HelloAck`, and prints everything that comes in.

```cpp
#include <Arduino.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace DryerUart;
using namespace idryer::hal;

// ============================================================================
// UART to MCU
// ============================================================================
constexpr int UART_RX_PIN = 16;  // ESP32 receives
constexpr int UART_TX_PIN = 17;  // ESP32 sends

// HAL wrapper around Serial1 (UART_NUM_1)
ArduinoSerial uartSerial(Serial1, /*uartNum*/ 1);
UartBridge    uartBridge;

uint32_t lastHeartbeatMs = 0;

// ============================================================================
// Incoming frame handlers
// ============================================================================

void onHello(const HelloPayload &p, const FrameHeader &h) {
    Serial.printf("[UART] Hello: role=%u, fw=%u.%u.%u, hw=%s, units=%u\n",
                  (unsigned)p.role,
                  (p.firmwareVersion >> 16) & 0xFF,
                  (p.firmwareVersion >> 8)  & 0xFF,
                  p.firmwareVersion         & 0xFF,
                  p.hardwareVersion,
                  p.unitsCount);
    Serial.printf("       mcuSerial=%s, deviceType=%u\n",
                  p.mcuSerial, p.deviceType);

    // Reply with HelloAck - IP/SSID stay zero because WiFi is not up yet
    HelloAckPayload ack{};
    ack.ipAddress = 0;
    ack.ssid[0]   = '\0';
    uartBridge.sendHelloAck(ack);
}

void onTelemetry(const TelemetryPayload &p, const FrameHeader &h) {
    for (uint8_t i = 0; i < p.count; i++) {
        const auto &u = p.units[i];
        Serial.printf("[UART] Telemetry: U%u T=%.1f°C H=%.1f%% heater=%u%% fan=%s\n",
                      u.unitId + 1,
                      u.temperatureC10 / 10.0f,
                      u.humidityPct10 / 10.0f,
                      u.heaterPowerPct,
                      u.fanOn ? "on" : "off");
    }
    uartBridge.sendTelemetryAck(h.sequence);
}

void onHeartbeat(const HeartbeatPayload &p, const FrameHeader &h) {
    Serial.printf("[UART] Heartbeat from MCU: uptime=%us, errors=%u\n",
                  p.uptimeSeconds, p.errorsSinceBoot);
}

void onError(const ErrorPayload &p, bool remote) {
    Serial.printf("[UART] %s error: code=%d seq=%u detail=%u\n",
                  remote ? "remote" : "local",
                  (int)p.code, p.lastSequence, p.detail);
}

// ============================================================================
// Setup & loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n== iDryer UART bridge ==");

    // HAL: logs through Serial
    initArduinoHal(&Serial);

    // UART1 to MCU
    Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    uartBridge.begin(&uartSerial, 115200);

    // Callbacks
    uartBridge.setHelloHandler(onHello);
    uartBridge.setTelemetryHandler(onTelemetry);
    uartBridge.setHeartbeatHandler(onHeartbeat);
    uartBridge.setErrorHandler(onError);

    // Trigger the MCU: "send your Hello with parameters"
    HelloPayload req{};
    req.role = Role::HelloRequest;     // 0xFF
    uartBridge.sendHello(req);

    Serial.println("Bridge ready. Waiting for MCU...");
}

void loop() {
    uartBridge.loop();

    // MCU heartbeat every 5 seconds (library constant)
    if (millis() - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        HeartbeatPayload hb{};
        hb.uptimeSeconds   = millis() / 1000;
        hb.wifiRssiDbm     = 0;           // WiFi is not up yet
        hb.errorsSinceBoot = 0;
        hb.cloudState      = 0;           // LinkCloudState::Idle
        uartBridge.sendHeartbeat(hb);
        lastHeartbeatMs = millis();
    }
}
```

---

## Step 3. Build and Upload

In VSCode: `PlatformIO -> Build -> Upload`, or in a terminal:

```bash
pio run -t upload
pio device monitor
```

### What You Should See in Serial Monitor

If the MCU is working correctly:

```
== iDryer UART bridge ==
Bridge ready. Waiting for MCU...
[UART] Hello: role=1, fw=1.0.0, hw=v1.0, units=1
       mcuSerial=36B955AB4350FEDC, deviceType=1
[UART] Telemetry: U1 T=55.3°C H=45.2% heater=80% fan=on
[UART] Telemetry: U1 T=55.3°C H=45.2% heater=80% fan=on
[UART] Heartbeat from MCU: uptime=12s, errors=0
...
```

If you only see `Bridge ready. Waiting for MCU...`:

- check the UART pins (`ESP32 RX <- MCU TX`, `ESP32 TX -> MCU RX`),
- check GND, which must be shared,
- check that the MCU is actually running and sending `Hello`,
- in iDryer Link, the MCU sends Hello only **after** the `Role::HelloRequest` trigger; the sketch above sends that trigger in `setup()`.

---

## What Is Under `UartBridge`

The library does this for you:

- Parser: collects bytes into a full frame, checks CRC, and filters out broken or too-long frames.
- Retry: if you send a frame with `ackRequired = true` and no ACK arrives within 700 ms, the library resends it up to 3 times.
- Fragmentation: JSON config chunks (`ConfigPush` = 0x30) are split into 194-byte pieces by the library.
- Dispatch: the handler for the current `KIND` is called automatically (`setTelemetryHandler`, `setCommandHandler`, ...).

Your code only decides what to do on receive and what to send.

---

## What Next

Now the ESP32 can see the MCU and understands the frames. The next step is cloud connectivity:

1. **WiFi** - connect the ESP32 to an access point.
2. **Provision and claim** - on the first run, the device must get a `deviceToken` and show the user a PIN. The standard flow is automated through `CloudStateMachine`.
3. **MQTT** - after the `Online` state, publish telemetry and status to the standard topics.

`CloudStateMachine` + `MqttClient` + `HttpApi` handle that whole part, as shown in [04-quickstart-standalone.md](04-quickstart-standalone.md) (the example there is without UART, but the classes are the same; the bridge also adds `UartCommandSink` and `CommandHandler`).

### Minimal Checklist Before Building the Full Bridge

- [ ] All UART handlers are registered: Hello, Telemetry, Status, Weights, Rfid, Command, ClaimStart, Heartbeat, Error, Log.
- [ ] When MQTT receives a command, `CommandHandler` calls `UartCommandSink`, and the sink calls `UartBridge::sendCommand()`.
- [ ] `mcuSerial` is saved when Hello arrives from the MCU, because it is needed for `info` in MQTT.
- [ ] The LINK heartbeat contains `cloudState` from `CloudStateMachine::getState()`.
- [ ] Each incoming Telemetry message is published to MQTT as JSON, either through `TelemetryPublisher` or manually.
- [ ] MQTT LWT is configured for `idryer/<serial>/offline`.

---

## Full Mass Production Flow

The reference implementation of this bridge is the **iDryer Link** firmware ([idryer-link repository](https://github.com/pavluchenkor/idryer-link)). It contains the whole stack: screen, menu, staging, OTA updates. Use it as a live reference - it is not a hard dependency, just a working example on the same library.

---

← [Back to paths](01-choose-your-path.md) | [Standalone on a single ESP32](04-quickstart-standalone.md) →
