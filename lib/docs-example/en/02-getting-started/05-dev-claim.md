# Device Claiming: Standard and Dev Paths

Claim is the process of binding a device to the user's account on the iDryer portal. The result is that the device gets a `deviceToken` (the MQTT password), the portal gets a database record tied to the user, and the device becomes visible in the app.

This document describes **three ways** to run claim, ordered from user-facing UX to dev testing:

1. [Path 1 - Through the Device Screen](#path-1-through-the-device-screen) (the standard iDryer path).
2. [Path 2 - Through the Web Installer](#path-2-through-the-web-installer-flasher-portal) (the standard path for headless devices).
3. [Path 3 - Through Serial Monitor](#path-3-through-serial-monitor-dev-test) (dev testing when flasher-portal is not ready yet).

All three variants use **the same HTTP API** - [05-cloud/02-http-api.md](../05-cloud/02-http-api.md). The only difference is how the user gets the PIN and how the result is shown.

!!! note "Context"
    The overall claim architecture (what the PIN is, why `deviceToken` exists, what the backend does) is covered in [05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md). The end-to-end scenario with UART frames is in [06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).

---

## Path 1 - Through the Device Screen

The standard method for devices with a local UI (iDryer).

### What the User Sees

1. The user turns on the device and connects it to WiFi through the menu.
2. In the menu, they choose **"Start claiming"** or the equivalent item.
3. The device shows an **8-digit PIN** on the screen, plus a 10-minute countdown.
4. The user opens the iDryer app or [portal.idryer.org](https://portal.idryer.org), taps "Add device", and enters the PIN.
5. A few seconds later, the device screen shows "Successfully claimed" plus the name the user entered in the app.

### What the Firmware Does

```cpp
// on the event "user pressed 'start claiming' in the menu"
cloud.requestClaim();
```

Everything else is handled by `CloudStateMachine`: it transitions through `Provisioning -> Registering -> AwaitingClaim -> Ready -> Online`. The firmware subscribes to callbacks and shows the PIN on screen:

```cpp
cloud.setClaimPinCallback([](const char* pin, uint32_t expiresInSeconds, void*) {
    display.showPin(pin, expiresInSeconds);
}, nullptr);

cloud.setClaimCompleteCallback([](const char* deviceId, void*) {
    display.showMessage("Connected!");
}, nullptr);
```

This method is used in the reference iDryer Link firmware - see `src/IdryerDevice.cpp`, handlers `onClaimPin` / `onClaimComplete`.

---

## Path 2 - Through the Web Installer (flasher-portal)

The standard method for devices **without a screen**, or when the user prefers to flash and claim in one click through the browser.

### What the User Sees

1. The user opens `portal.idryer.org` in a browser with Web Serial support (Chrome/Edge).
2. The user connects the device over USB.
3. The user clicks "Flash" - flasher-portal uploads the firmware through Web Serial.
4. After reboot, the device reports its `mcuSerial` over Serial:
   ```
   RP2040_SERIAL:36B955AB4350FEDC
   ```
5. The portal catches that line, calls `POST /devices/provision` + `/register`, and gets a PIN.
6. The PIN is shown **directly in the browser** - the user sees it next to the device and enters it in the app, or the portal does it automatically if the user is already signed in.

### What the Firmware Does

In the firmware, on the first boot after flashing, send `mcuSerial` over Serial so the flasher can read it:

```cpp
// in IdryerDevice::handleRpHello or in setup()
Serial.printf("RP2040_SERIAL:%s\n", mcuSerial);
```

The claim cycle itself is the same `cloud.requestClaim()` flow, or an automatic start when NVS is empty. The PIN arrives through a callback, and the firmware can print it to the same Serial for installer convenience:

```cpp
cloud.setClaimPinCallback([](const char* pin, uint32_t expiresIn, void*) {
    Serial.printf("CLAIM_PIN:%s\n", pin);
}, nullptr);
```

The flasher portal catches `CLAIM_PIN:...` and renders it nicely in the browser UI.

### When It Is Unavailable

If flasher-portal **does not know your device type yet** - for example, you are building a new product such as iHeater, LinkII, or your own device - this path will not work for now. Use [Path 3](#path-3-through-serial-monitor-dev-test) until you wire the new type into the flasher.

---

## Path 3 - Through Serial Monitor (Dev Test)

Use this when:

- The device is still under development, so there is no screen yet or the claiming UI is not written yet.
- flasher-portal is not configured for your device type.
- You want to verify MQTT connectivity quickly without UI work.

### The Idea

Any firmware based on `idryer-protocol` can accept the `claim` command **from the Serial port** - that is five lines of code. In Serial Monitor you type `claim`, the device starts the cycle, and the PIN appears right in the log. You enter the PIN manually in the iDryer web app - done.

### Minimal Firmware Code

In `loop()`:

```cpp
void loop() {
    cloud.loop();
    // ... other work ...

    // Dev trigger for claim through Serial. Remove in production.
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "claim") {
            Serial.println("[USER] Starting claim...");
            cloud.requestClaim();
        }
    }
}
```

And the PIN callback, so you can see it in the log if it is not already there:

```cpp
// in setup(), alongside the rest of the cloud configuration
cloud.setClaimPinCallback([](const char* pin, uint32_t expiresIn, void*) {
    Serial.println();
    Serial.println("================================");
    Serial.printf("  PIN: %s\n", pin);
    Serial.printf("  Enter it in the iDryer app\n");
    Serial.printf("  Valid for: %u sec\n", expiresIn);
    Serial.println("================================");
    Serial.println();
}, nullptr);

cloud.setClaimCompleteCallback([](const char* deviceId, void*) {
    Serial.printf("[CLOUD] Claimed! deviceId=%s\n", deviceId);
}, nullptr);
```

The full working example is `examples/esp32_standalone/esp32_standalone.ino` in the root of the library repository. That example runs as is and shows the full chain.

### Developer Sequence

1. **Upload the firmware** with PlatformIO (`pio run -t upload`) or any flasher.
2. **Open Serial Monitor** at 115200 baud.
3. Wait for WiFi to connect:
   ```
   [CLOUD] State: WifiConnecting -> Ready
   ```
4. **Type `claim` and press Enter**. The device responds:
   ```
   [USER] Starting claim...
   [CLOUD] State: Ready -> Provisioning
   [CLOUD] State: Provisioning -> Registering
   [CLOUD] State: Registering -> AwaitingClaim

   ================================
     PIN: 12345678
     Enter it in the iDryer app
     Valid for: 600 sec
   ================================
   ```
5. Open [portal.idryer.org](https://portal.idryer.org) in a browser, sign in, choose "Add device", enter PIN `12345678`, and give the device a name.
6. The Serial log shows confirmation:
   ```
   [CLOUD] State: AwaitingClaim -> Ready
   [CLOUD] Claimed! deviceId=3fa85f64-...
   [CLOUD] State: Ready -> MqttConnecting
   [CLOUD] State: MqttConnecting -> Online
   ```

The device is visible in the app, telemetry flows, and commands are accepted.

### Auto-Claim on First Boot

If you do not want to type `claim` into the terminal every time, you can automate it: the device starts the cycle itself if NVS does not contain a token:

```cpp
void setup() {
    // ... usual initialization ...
    cloud.begin();

    // First boot: NVS is empty -> start claim automatically
    if (!cloud.getIdentity().hasToken()) {
        Serial.println("[DEV] No token in NVS - auto claim");
        cloud.requestClaim();
    }
}
```

After claiming, `deviceToken` is stored in NVS, and on the next boot the device goes **straight** to `WifiConnecting -> Ready -> MqttConnecting -> Online` without a new PIN.

### How to Reset Claim for Re-Testing

If you want to test the flow again, clear the token in NVS:

```cpp
// insert once in setup() before cloud.begin()
store.clear();  // ArduinoCredentialStore::clear() - clears everything

// or only the serial/token through Preferences.h
// Preferences prefs;
// prefs.begin("idryer", false);
// prefs.clear();
// prefs.end();
```

Or use the device menu, if it has one - "Reset settings".

**Portal alternative:** the user deletes the device in the app. After that, a repeated `claim` on the device will issue a new PIN and bind it again.

---

## What Matters About Errors

| Symptom | Cause | Fix |
|---|---|---|
| `State: AwaitingClaim` hangs forever | The user did not enter the PIN within 10 minutes | The PIN expired. The device will repeat `register` or wait for a command. Restart claim. |
| `provision returned deviceToken: null, isClaimed: true` | The device is already claimed in the portal | The user must remove the device in the app, then claim it again. |
| `State: Error` | The backend is unavailable or WiFi died during the request | Check internet access on the device and repeat `claim`. |
| The PIN is present in Serial, but the app says "invalid PIN" | Wrong digits or the PIN expired | Re-check it and restart claim if it expired. |

The full `CloudState` and transition reference is in [05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md).

---

## What Next

- [Main flows -> Claiming](../06-flows/02-claiming-flow.md) - the end-to-end scenario with UART frames for teams building the controller + LINK as two boards.
- [Portal HTTP API](../05-cloud/02-http-api.md) - details of the `/provision`, `/register`, `/claim`, and `/check-claim` endpoints.
- [Standard commands](../04-mqtt/04-backend-to-device.md) - what is available after a successful claim.
