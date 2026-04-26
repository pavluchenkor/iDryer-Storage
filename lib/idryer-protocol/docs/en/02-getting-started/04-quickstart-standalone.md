# Quickstart: One ESP32 for Everything (Standalone)

The minimal path for a device where one ESP32 both controls the hardware and talks to the cloud. There is no UART between boards.

Typical cases: telemetry module, iHeater-like devices, simple single-loop dryers.

!!! note "Context"
    Before this quickstart, it helps to understand the UART protocol: `CommandPayload` and `ProfilePayload` are part of the cloud contract, and you will need to apply them locally. Quick overview: [../01-overview/03-nodes-and-roles.md](../01-overview/03-nodes-and-roles.md). Details: [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

---

## Quickstart Goal

The ESP32 goes through the full "out of the box -> online in the cloud" flow:

1. Connects to WiFi.
2. Runs `provision` and gets a `deviceToken`.
3. Gets a PIN, and the user enters it in the app.
4. Waits for claiming to finish.
5. Connects to MQTT.
6. Publishes telemetry and status.
7. Receives commands and applies them locally, without UART.

The main engine is `CloudStateMachine`.

---

## What You Need

- An ESP32 board.
- An account on [portal.idryer.org](https://portal.idryer.org).
- A WiFi network with internet access.
- PlatformIO.

---

## Step 1. `platformio.ini`

```ini
[env:esp32dev]
platform   = espressif32
board      = esp32dev
framework  = arduino
monitor_speed = 115200

build_flags =
  -DIDRYER_API_BASE=\"https://portal.idryer.org/api\"

lib_deps =
  https://github.com/pavluchenkor/idryer-protocol.git
  bblanchon/ArduinoJson @ ^7.0.4
```

`IDRYER_API_BASE` sets the base REST API URL. In production it is `https://portal.idryer.org/api`, and on a local backend without nginx it is `http://localhost:3000`.

---

## Step 2. `src/main.cpp`

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace idryer;
using namespace idryer::cloud;

// ============================================================================
// Configuration
// ============================================================================
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
const char* API_BASE      = IDRYER_API_BASE;   // set in platformio.ini

// ============================================================================
// Components
// ============================================================================
ArduinoWifiManager     wifiMgr;
ArduinoHttpClient      httpClient;
ArduinoCredentialStore credStore;

HttpApi            api(&httpClient, API_BASE);
MqttClient         mqtt;
CloudStateMachine  cloud(&wifiMgr, &credStore, &api, &mqtt);

uint32_t lastTelemetryMs = 0;

// ============================================================================
// serialNumber from the ESP32 MAC address
// (in a two-MCU setup the serial comes from the MCU in Hello; here there is no MCU)
// ============================================================================
void makeSerialFromMac(char *buf, size_t bufSize) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(buf, bufSize, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ============================================================================
// Cloud callbacks
// ============================================================================

void onStateChange(CloudState oldS, CloudState newS, void*) {
    Serial.printf("[CLOUD] %s -> %s\n",
                  cloudStateToString(oldS),
                  cloudStateToString(newS));
}

void onClaimPin(const char *pin, uint32_t expiresIn, void*) {
    Serial.println();
    Serial.println("================================");
    Serial.printf("  PIN: %s\n", pin);
    Serial.printf("  Enter it in the iDryer app\n");
    Serial.printf("  Valid for: %u sec\n", expiresIn);
    Serial.println("================================");
    Serial.println();
}

void onClaimComplete(const char *deviceId, void*) {
    Serial.printf("[CLOUD] Device claimed. deviceId=%s\n", deviceId);
}

void onUnclaimed(void*) {
    Serial.println("[CLOUD] Device is not claimed. Type 'claim' in Serial Monitor.");
}

// ============================================================================
// MQTT commands from the backend
// ============================================================================

void onMqttCommand(const char *cmd, JsonObjectConst data) {
    Serial.printf("[MQTT] cmd=%s\n", cmd);

    if (strcmp(cmd, "drying") == 0) {
        uint8_t unitId = data["unitId"] | 0;
        int temp       = data["params"]["temperature"] | 50;
        int minutes    = data["params"]["duration"]    | 60;
        Serial.printf("  -> DRYING U%d  target=%d°C  duration=%d min\n",
                      unitId + 1, temp, minutes);
        // Start PID, timer, and set target here...
    }
    else if (strcmp(cmd, "stop") == 0) {
        Serial.println("  -> STOP");
        // Turn off the heater, reset the timer...
    }
    else if (strcmp(cmd, "find") == 0) {
        Serial.println("  -> FIND (screen/LED blink)");
    }
    // See docs/04-mqtt/04-backend-to-device.md for the remaining commands
}

// ============================================================================
// Telemetry publishing
// ============================================================================

void publishTelemetry() {
    // Replace with real sensor data
    float temp     = 25.0f + (random(0, 50) / 10.0f);
    float humidity = 40.0f + (random(0, 20));
    uint8_t heater = 0;
    bool fan       = false;

    StaticJsonDocument<256> doc;
    char ts[32];
    doc["timestamp"] = MqttClient::getIsoTimestamp(ts);

    JsonArray units = doc.createNestedArray("units");
    JsonObject u = units.createNestedObject();
    u["unitId"]      = "U1";
    u["temperature"] = temp;
    u["humidity"]    = humidity;
    u["heaterPower"] = heater;
    u["fanStatus"]   = fan;

    mqtt.publishTelemetry(doc);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n== iDryer standalone ==");

    initArduinoHal(&Serial);

    wifiMgr.begin(WIFI_SSID, WIFI_PASSWORD);

    // Cloud callbacks
    cloud.setStateChangeCallback(onStateChange, nullptr);
    cloud.setClaimPinCallback(onClaimPin, nullptr);
    cloud.setClaimCompleteCallback(onClaimComplete, nullptr);
    cloud.setUnclaimedCallback(onUnclaimed, nullptr);

    // MQTT commands
    mqtt.setCommandCallback(onMqttCommand);

    // Standalone: no UART, serial comes from MAC
    cloud.setWaitForMcuSerial(false);
    cloud.begin();

    char serial[13];
    makeSerialFromMac(serial, sizeof(serial));
    cloud.setMcuSerial(serial);

    Serial.printf("[INIT] serialNumber: %s\n", serial);
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    cloud.loop();

    // Interactive claim through Serial
    if (Serial.available()) {
        String in = Serial.readStringUntil('\n');
        in.trim();
        if (in == "claim") {
            Serial.println("[USER] requestClaim");
            cloud.requestClaim();
        }
    }

    // Publish only when online
    if (!cloud.isOnline()) return;

    uint32_t now = millis();
    if (now - lastTelemetryMs >= 5000) {
        publishTelemetry();
        lastTelemetryMs = now;
    }
}
```

---

## Step 3. Start and Claim the Device

### 3.1. First build and upload

```bash
pio run -t upload
pio device monitor
```

### 3.2. What Happens Step by Step (Serial Monitor)

```
== iDryer standalone ==
[INIT] serialNumber: AABBCCDDEEFF
[CLOUD] Idle -> WifiConnecting
[CLOUD] WifiConnecting -> Provisioning
[CLOUD] Provisioning -> Registering
[CLOUD] Registering -> AwaitingClaim

================================
  PIN: 12345678
  Enter it in the iDryer app
  Valid for: 600 sec
================================
```

### 3.3. Claim in the App

1. Sign in to [portal.idryer.org](https://portal.idryer.org) or use the mobile app.
2. Tap "Add device" and enter PIN `12345678`.
3. A few seconds later, Serial Monitor shows:

```
[CLOUD] AwaitingClaim -> Ready
[CLOUD] Device claimed. deviceId=3fa85f64-5717-4562-b3fc-2c963f66afa6
[CLOUD] Ready -> MqttConnecting
[CLOUD] MqttConnecting -> Online
```

Now the device tab in the portal shows your `temperature` and `humidity`.

### 3.4. Re-run

After the first claim, `deviceToken` is stored in NVS. On the next boot the device goes straight to `WifiConnecting → Ready → MqttConnecting → Online`, with no PIN.

---

## Backend Commands

At this point, the sketch only prints parameters to Serial when it receives `drying`. The next step is to apply them:

```cpp
void onMqttCommand(const char *cmd, JsonObjectConst data) {
    if (strcmp(cmd, "drying") == 0) {
        uint8_t unitId = data["unitId"] | 0;
        float tempC    = data["params"]["temperature"] | 50;
        int durMinutes = data["params"]["duration"]    | 60;

        setTargetTemperature(tempC);          // your PID
        startDryingTimer(durMinutes);
        setHeaterEnable(true);
        currentMode = DryerMode::Drying;
    }
    // ...
}
```

See the JSON shape for each command in [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md).

### Alternative: `CommandHandler` + Your Own `ICommandSink`

Instead of parsing JSON directly, you can use the ready-made `CommandHandler`. It parses the JSON and calls your `ICommandSink` methods with ready binary structures (`CommandPayload`, `ProfilePayload`). This is useful if you want to reuse the same command application logic in standalone and in a two-MCU setup. Details: [../05-cloud/03-command-sink.md](../05-cloud/03-command-sink.md).

---

## What to Publish Besides Telemetry

For a fully functional device that is visible in the portal, you minimally need:

| Topic | When to publish | Method |
|-------|-------------------|-------|
| `info` | Once after Online (retained) | `mqtt.publishInfo(doc)` |
| `telemetry` | Every 5 seconds | `mqtt.publishTelemetry(doc)` |
| `status` | When the mode changes (retained) | `mqtt.publishStatus(doc)` |

JSON formats and required fields: [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md).

---

## Readiness Checklist

- [ ] The ESP32 connects to WiFi successfully.
- [ ] The PIN is visible in Serial Monitor.
- [ ] After entering the PIN, the device switches to `Online`.
- [ ] The latest telemetry is visible in the portal.
- [ ] The `stop` / `drying` command from the app is logged on the device.
- [ ] `info` is published as retained, so the device appears immediately after the portal restarts.
- [ ] `LWT` (broker-published `offline`) works, and the UI shows "offline" when power is cut.

---

← [Back to paths](01-choose-your-path.md) | [UART protocol in detail](../03-uart/) →
