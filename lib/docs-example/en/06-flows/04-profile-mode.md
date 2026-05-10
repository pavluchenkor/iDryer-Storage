# Profile drying: from MQTT command to PID

Complete profile-mode flow: the user defines a sequence of stages in the app, the command arrives on the device, the MCU applies it with ramp/hold logic, tracks progress, and reports transitions in `status`.

---

## What a profile is

A profile is a **sequence of temperature stages** with ramp-rate control. Each stage is defined by three numbers:

- **`temp`** — target temperature in °C.
- **`ramp`** — the number of seconds over which the MCU must smoothly raise or lower the temperature to `temp`.
- **`hold`** — the number of seconds to keep `temp` after it is reached.

Up to 10 stages. It is used for “advanced” drying: gentle heat-up, soak, intensive mode, and cooldown.

---

## Start command

**Topic:** `idryer/<serial>/commands/profile`, QoS 1.

```json
{
  "unitId": "U1",
  "mode": "PROFILE",
  "startStage": 0,
  "stages": [
    { "temp": 60,  "ramp": 300,  "hold": 1800 },
    { "temp": 100, "ramp": 600,  "hold": 6000 },
    { "temp": 70,  "ramp": 600,  "hold": 12000 }
  ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

Required fields: `unitId`, `stages`. The format of each stage is `temp`/`ramp`/`hold`. See [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) for details.

---

## UART transport

`CommandHandler` builds a `ProfilePayload` (64 bytes) and calls `sink->sendProfileCommand(payload, true)`. `UartCommandSink` → `UartBridge::sendProfileCommand` → `MessageKind::Command (0x20)` frame with a **64-byte payload**.

```
 LINK                                      MCU
  │                                         │
  │ ──Command (0x20, LEN=64, ProfilePayload)──►
  │                                         │
  │                               MCU distinguishes by LEN:
  │                               13 → CommandPayload
  │                               64 → ProfilePayload  ← our case
  │                                         │
  │ ◄──CommandAck(seq, OK)──────────────────
  │                                         │
```

The MCU must check `payloadLength == 64`, not the payload fields.

The exact byte layout is in [../03-uart/04-binary-structures.md#profilepayload-inside-command-0x20--64-bytes](../03-uart/04-binary-structures.md). A hex dump of a working frame is in [../03-uart/06-examples.md](../03-uart/06-examples.md) (section “Command with ProfilePayload”).

---

## MCU logic

Heating is controlled by a PID loop with a changing target temperature.

### State variables

```c
struct ProfileState {
    uint8_t  currentStage;      // Current stage (0-based)
    uint8_t  totalStages;       // Total number of stages
    float    prevTemp;          // Temperature at stage start
    uint32_t stageStartTime;    // millis() at stage start
    uint32_t holdStartTime;     // millis() at the start of the HOLD phase
    bool     inRamp;            // true = RAMP, false = HOLD
};
```

### Stage automaton

```
┌──────────────────────────────────────────────────────────────┐
│                       PROFILE MODE                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌─────────┐    temp reached    ┌─────────┐              │
│   │  RAMP   │ ─────────────────►  │  HOLD   │              │
│   │         │    or ramp expired  │         │              │
│   └────┬────┘                     └────┬────┘              │
│        │ stageTime >= ramp             │ holdTime >= hold   │
│        │   (if ramp > 0)               │                    │
│        ▼                               ▼                    │
│    switch to HOLD             next stage                    │
│                                or finish                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Main tick pseudocode

```c
void profileTick(uint32_t now) {
    Stage *stage = &stages[currentStage];
    uint32_t stageTime = now - stageStartTime;

    if (inRamp) {
        if (stage->ramp == 0) {
            // Forced heat-up: heat at maximum power
            targetTemp = stage->temp;
            heaterPower = MAX_POWER;

            // Switch to HOLD when the target is reached (with hysteresis)
            if (currentTemp >= stage->temp - HYSTERESIS) {
                inRamp = false;
                holdStartTime = now;
            }
        } else {
            // Linear ramp: target increases over time
            if (stageTime < stage->ramp * 1000) {
                float progress = (float)stageTime / (stage->ramp * 1000);
                targetTemp = prevTemp + (stage->temp - prevTemp) * progress;
            } else {
                // Ramp is complete
                targetTemp = stage->temp;
                inRamp = false;
                holdStartTime = now;
            }
        }
    } else {
        // HOLD: maintain target temperature
        targetTemp = stage->temp;
        uint32_t holdTime = now - holdStartTime;

        if (holdTime >= stage->hold * 1000) {
            // Stage is complete
            if (currentStage + 1 < totalStages) {
                prevTemp = currentTemp;
                currentStage++;
                stageStartTime = now;
                inRamp = true;
            } else {
                // Profile is complete
                setMode(IDLE);
                return;
            }
        }
    }

    heaterPower = pidCompute(targetTemp, currentTemp);
}
```

### Special case: `ramp = 0`

“Forced heat-up”: the MCU heats at maximum power until `temp` is reached. The `hold` timer starts **after** the target is reached, not from the start of the stage. This is useful when you need “30 minutes at 60 °C”, not “30 minutes including ramp-up”.

---

## Progress publication

While the profile is running, `status` is updated **on every stage change** and on `RAMP ↔ HOLD` transitions. For additional visibility, the MCU can publish `status` more often, for example every 10 seconds.

`status` fields in PROFILE mode are described in [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md):

- `currentStage` (0-based)
- `totalStages`
- `stageElapsed` / `stageRemaining`
- `stagePhase` — `"RAMP"` or `"HOLD"`
- `totalElapsed` / `totalRemaining` — for the whole program

Example (1 hour into stage 2):

```json
{
  "units": [{
    "unitId": "U1",
    "mode": "PROFILE",
    "sessionNum": 42,
    "target": { "temperature": 100.0, "duration": 0 },
    "totalElapsed": 4200,
    "totalRemaining": 10200,
    "currentStage": 1,
    "totalStages": 3,
    "stageElapsed": 600,
    "stageRemaining": 5400,
    "stagePhase": "HOLD"
  }],
  "uptime": 12345
}
```

---

## Interruption

The `stop` command:

```json
{ "unitId": "U1" }
```

interrupts the profile immediately: the MCU switches to Idle and the heater turns off. `status` changes to `"IDLE"` (short form).

---

## Profile examples

### Standard PLA drying

```json
{
  "unitId": "U1",
  "stages": [
    { "temp": 50, "ramp": 0, "hold": 14400 }
  ]
}
```

Fast heat-up to 50 °C, then hold for 4 hours.

### Intensive ABS drying

```json
{
  "unitId": "U1",
  "stages": [
    { "temp": 65, "ramp": 600,  "hold": 3600 },
    { "temp": 80, "ramp": 300,  "hold": 7200 },
    { "temp": 65, "ramp": 600,  "hold": 3600 }
  ]
}
```

10 min warm-up → 1 hour at 65 °C → 5 min fast rise → 2 hours at 80 °C → 10 min cooldown → 1 hour at 65 °C.

### Gentle nylon drying

```json
{
  "unitId": "U1",
  "stages": [
    { "temp": 40, "ramp": 1200, "hold": 7200 },
    { "temp": 70, "ramp": 1800, "hold": 28800 },
    { "temp": 50, "ramp": 1200, "hold": 3600 }
  ]
}
```

Very gentle heat-up to avoid thermal shock.

---

## Next steps

- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) — JSON format for the profile command.
- [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md) — byte-for-byte `ProfilePayload`.
- [../03-uart/06-examples.md](../03-uart/06-examples.md) — real frame hex dump.
