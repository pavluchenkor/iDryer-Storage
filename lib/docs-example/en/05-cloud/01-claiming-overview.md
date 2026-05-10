# Claiming Overview

This document explains **why** claiming exists, **who** participates in it, and **what** event sequence occurs. The technical HTTP and UART contract is described in the adjacent documents.

!!! note "Context"
    Before reading, make sure the MCU/LINK/Portal roles are clear - [../01-overview/03-nodes-and-roles.md](../01-overview/03-nodes-and-roles.md). The `serialNumber`, `deviceToken`, `deviceId`, and user JWT identifiers are described in [../01-overview/04-glossary.md](../01-overview/04-glossary.md).

---

## Why claiming is needed

An iDryer device has **no** QR code, no hardcoded password, and no factory-side claiming dispatcher. The device is assembled, flashed, and shipped to the user. The user:

1. Turns on the device.
2. Connects it to their WiFi.
3. Sees a PIN on the screen.
4. Enters the PIN in the app or portal.

After that, the device is **bound to the user's account**. All subsequent data and commands go through that account.

Claiming is the mechanism for secure device binding without physical access to the device.

---

## Participants

- **MCU** - starts the process from the menu button ("start claiming") and shows the PIN on the display.
- **LINK** - sends HTTP requests to the portal, stores `deviceToken` in NVS, and tracks state.
- **Portal** (backend + database) - issues tokens and PINs, creates `Link` and `Device` records, and checks permissions.
- **User** - enters the PIN in the app to connect the device to their account.

---

## `Link` states in the portal database

Claiming changes the state of the `Link` record in the portal database:

| State | When | Meaning |
|-------|------|---------|
| `UNCLAIMED` | After the first provision | The token has been issued, but the user has not claimed yet |
| `CLAIMED` | After `POST /devices/claim` | The user has claimed the device, but the first MQTT `info` has not arrived yet |
| `BOUND` | After the first MQTT `info` from an online device | A fully active bound device |

The `CLAIMED → BOUND` transitions are automatic on the first MQTT connection.

The portal also has a separate `Device` table - the dryer card record (model, name). One `Link` can be bound to one `Device`. Unlinking in the app resets the `Link` back to `UNCLAIMED`.

---

## Sequence, in words

1. **MCU tells LINK: "start claiming"** - UART frame `ClaimStart (0x70)`.
2. **LINK calls `POST /devices/provision`** with `serialNumber`. It receives `deviceToken` and stores it in NVS.
3. **LINK calls `POST /devices/register`** with `token`. It receives a PIN (8 digits, 10 minute TTL).
4. **LINK passes the PIN to MCU** through the `ClaimStatus (0x71)` UART frame. MCU shows it on the display.
5. **The user enters the PIN and device name in the app**. The app calls `POST /devices/claim` with `Authorization: Bearer <user JWT>`.
6. **Portal** links `Link` to the user's account, creates a `Device` record, and moves `Link` to `CLAIMED`.
7. **LINK periodically polls `GET /devices/check-claim/:token`**. Before claiming - `404`. After claiming - `200 OK` with `deviceId`.
8. **LINK receives the response and sends `ClaimComplete (0x72)` to MCU** with success=1 and `deviceId` for display.
9. **LINK connects to MQTT** with `(serialNumber, deviceToken)`.
10. **LINK publishes `info` to MQTT** - the portal moves the `Link` to `BOUND`.

After that, the device is visible in the user's app, telemetry flows, and commands are accepted.

---

## Diagram

```
   User           MCU            LINK              Portal
   (app)                                       (backend + MQTT)
       │           │              │                    │
       │      ┌────┴──────┐       │                    │
       │      │ Pressed:  │       │                    │
       │      │ "start    │       │                    │
       │      │ claiming" │       │                    │
       │      └────┬──────┘       │                    │
       │           │ ClaimStart   │                    │
       │           ├────UART────►│                    │
       │           │              │ POST /provision    │
       │           │              ├───HTTP────────────►│
       │           │              │◄───{deviceToken}───┤
       │           │              │ POST /register     │
       │           │              ├───HTTP────────────►│
       │           │              │◄───{pin, expires}──┤
       │           │ ClaimStatus  │                    │
       │           │◄────UART─────┤                    │
       │      ┌────┴──────┐       │                    │
       │      │ Shows PIN │       │                    │
       │      │ on the    │       │                    │
       │      │ display   │       │                    │
       │      └───────────┘       │                    │
       │ "PIN: 12345678"          │                    │
       │ "Name: My Dryer"         │                    │
       │                          │                    │
       │ POST /claim Bearer JWT    │                    │
       ├────────HTTP───────────────┼───────────────────►│
       │                          │                    │
       │                          │ GET /check-claim   │
       │                          ├───HTTP (periodic)►│
       │                          │◄────{claimed, id}──┤
       │           │ ClaimComplete│                    │
       │           │◄────UART─────┤                    │
       │           │              │                    │
       │           │              │ MQTT CONNECT       │
       │           │              ├───────────────────►│
       │           │              │ publish info       │
       │           │              ├───────────────────►│
       │ (device visible in the app)                  │
       └──────────────────────────┼────────────────────┘
                                  │
                             online, ready
```

---

## Special cases

### Device is already claimed (recovery)

If `deviceToken` is erased (board replacement, NVS reset) and `serialNumber` matches an already claimed device, a repeated `provision` returns:

```json
{ "deviceToken": null, "isClaimed": true, "serialNumber": "..." }
```

LINK must show the user: "the device is already claimed, unlink it in the app first." After unlinking, provision will issue a new token again.

Details: [02-http-api.md](02-http-api.md) and the portal documentation (`LINK_CLAIM_SCENARIOS.md`).

### PIN expired

The PIN is valid for 10 minutes. After it expires, MCU/LINK can request a new one with another `POST /devices/register`. The portal returns the same PIN if the TTL is still active, or a new one if the old one has expired.

### Claim timeout (the user did not enter the PIN)

LINK polls `check-claim` every **`IDRYER_CLAIM_POLL_INTERVAL_MS = 5 seconds`** (a constant in `src/core/config.h`).

The overall timeout for "how long to wait for the user to enter the PIN" is **not fixed in the library** - that is the responsibility of the LINK application firmware. Typical UX:

- While the PIN is valid (10 minute TTL from the portal) - keep polling.
- If `remainingSeconds` from `/register` expires and the user does nothing - request a new PIN or send `ClaimComplete(success=0)` to MCU.

The exact values in the reference iDryer Link firmware are in the `idryer-link` repository.

### Reclaiming the same device

The user can unlink the device in the app, which returns `Link` to `UNCLAIMED` and invalidates `deviceToken`. On the next `ClaimStart`, a new `deviceToken` and a new PIN are issued.

---

## Security

- `deviceToken` is valid only for one `serialNumber`.
- `POST /devices/provision` does not require authentication, but it is protected by a rate limit (10 requests / 60 seconds).
- After claiming, a repeated `provision` with the same serial **does not issue a token** - this protects against an attacker who knows the serial trying to hijack the device.
- The user JWT is short-lived (usually hours); do not confuse it with `deviceToken` (persistent, stored in the device NVS).

See the portal repository `docs/development/PROVISION_SECURITY_DESIGN.md` for details on the threat matrix and design.

---

## What's next

- [02-http-api.md](02-http-api.md) - exact HTTP endpoints, request bodies, and response codes.
- [03-command-sink.md](03-command-sink.md) - the `ICommandSink` interface for routing parsed commands to UART or applying them locally.
- [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md) - the same scenario, but with UART frame bytes and the full step-by-step script.
