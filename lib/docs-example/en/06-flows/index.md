# End-to-end flows

Four typical scenarios that trace data through all three layers (MCU ↔ UART ↔ LINK ↔ MQTT ↔ Cloud). This is not just a reference, but a **sequence of events over time**.

| Document | Scenario |
|----------|----------|
| [01-first-boot.md](01-first-boot.md) | First boot: from power-on to `Online`, with a second-by-second timeline |
| [02-claiming-flow.md](02-claiming-flow.md) | Full claiming cycle: `ClaimStart` → HTTP → PIN → claim → `ClaimComplete` → MQTT |
| [03-remote-config.md](03-remote-config.md) | Menu config: full JSON, delta, `set`, `invoke` (UART ConfigPush + MQTT) |
| [04-profile-mode.md](04-profile-mode.md) | Profile drying: MQTT → ProfilePayload → PID RAMP/HOLD automaton on the MCU |

## When to read this

- You are studying how the device works as a whole for the first time → [01-first-boot.md](01-first-boot.md).
- You are figuring out why the device does not appear in the app → [02-claiming-flow.md](02-claiming-flow.md).
- You are changing settings through the menu and want to understand the flow → [03-remote-config.md](03-remote-config.md).
- You are implementing multi-stage drying on the MCU → [04-profile-mode.md](04-profile-mode.md).

## Related

- [../03-uart/](../03-uart/index.md) — UART frame reference used in these flows.
- [../04-mqtt/](../04-mqtt/index.md) — JSON formats used in these flows.
- [../05-cloud/](../05-cloud/index.md) — HTTP part.
