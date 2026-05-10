# UART: Physical Layer

Description of the electrical side of the link between MCU and LINK. If you are building the board, start here.

!!! note "Context"
    MCU/LINK roles and terminology are described in [../01-overview/03-nodes-and-roles.md](../01-overview/03-nodes-and-roles.md). A quick code example is in [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md).

---

## Link Parameters

| Parameter | Value |
|----------|-------|
| Baud rate | **115200** |
| UART frame format | **8N1** (8 data bits, no parity, 1 stop bit) |
| Hardware flow control | **not used** (RTS/CTS are not needed) |
| Direction | full duplex (both sides transmit at the same time) |
| Levels | **3.3 V logic** (matches ESP32 outputs) |

The 115200/8N1 values are hard-coded in both firmwares. Changing them requires rebuilding both the MCU and LINK sides - the contract is fixed in `src/uart/uart_protocol.h` and confirmed in `uart_bridge.cpp`.

!!! warning "5 V Levels"
    If your MCU runs at 5 V (classic Arduino Uno/Mega on ATmega328), the ESP32 input does not tolerate 5 V - you will burn the pin. Use a **bidirectional level shifter** (TXS0108, BSS138 + pull-ups) or a voltage divider on TX -> RX ESP32.

---

## Wiring Diagram

```
           MCU (3.3 V)           LINK (ESP32, 3.3 V)
          ┌────────┐            ┌──────────┐
          │   TX   ├────────────┤   RX     │    MCU -> LINK transmit line
          │   RX   ├────────────┤   TX     │    LINK -> MCU transmit line
          │   GND  ├────────────┤   GND    │    common ground (required)
          └────────┘            └──────────┘
```

The two boards may use independent power supplies, but **common ground is required**. Without it, transmission either will not work at all or will fail intermittently.

### If the MCU Runs at 5 V

```
       Arduino Mega (5 V)         ESP32 (3.3 V)
         ┌─────────┐             ┌─────────┐
         │ TX pin18├──┬────────┬─┤   RX    │
         │         │  │Level-  │ │         │
         │ RX pin19├──┤shifter ├─┤   TX    │
         │    GND  ├──┤(2-ch)  ├─┤   GND   │
         └─────────┘  └────────┘ └─────────┘
                         │
                      GND on both sides
```

An alternative for TX MCU -> RX ESP32 only (one direction) is a simple `1 kΩ + 2 kΩ` divider to ground.

---

## Signal Quality Requirements

- Line length - up to about 30 cm on ordinary jumper wires. For longer runs, use twisted pair or shielded cable, and consider lowering the baud rate.
- No capacitors on the data line - they slow down edges and break the bit rate.
- If the wires run near noisy heater power lines, add a `10 kΩ` pull-up to VCC on both RX lines. This is not mandatory, but it helps against induced noise.

---

## First Start: What to Check

Before you debug the protocol, make sure the physical layer works:

1. **Pinout:** `MCU TX -> LINK RX` (crossover). A common mistake is connecting TX -> TX.
2. **Common ground:** required, even if the power supply is shared.
3. **Speed:** both sides must be at 115200. Many libraries default to 9600.
4. **UART port:** `Serial` is usually occupied by USB debug; for board-to-board communication use `Serial1`/`Serial2`. See the pin table in [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md).

If the physical layer is fine, any terminal (Hercules, `screen`, `minicom`) will show `0xAA ...` on the other side after you send a frame - **not necessarily** a fully valid frame, but those exact bytes.

---

## What Next

- [02-frame-and-crc.md](02-frame-and-crc.md) - how the frame is structured and how CRC is calculated.
- [03-message-types.md](03-message-types.md) - all message types.
- [04-binary-structures.md](04-binary-structures.md) - offsets of all structures.
