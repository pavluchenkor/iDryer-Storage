# Claiming: full flow with UART bytes

Claiming the device to the user account. Step by step, with UART frames, HTTP requests, and state on each side.

!!! note "Context"
    Overview, why this exists, and portal states are in [../05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md). The HTTP contract is in [../05-cloud/02-http-api.md](../05-cloud/02-http-api.md). UART `ClaimStart/Status/Complete` frames are in [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

---

## Initial state

- MCU and LINK are powered on, and the UART handshake has completed (see [01-first-boot.md](01-first-boot.md)).
- LINK is connected to WiFi.
- `deviceToken` is **absent** in NVS.
- The user has an account on portal.idryer.org and is logged in to the app.

---

## Step 1. The user initiates claiming

The user selects “start claiming” in the MCU menu (the exact UI is up to you).

**MCU → LINK:**

```
MCU ──UART ClaimStart (0x70, LEN=0, FLAGS=ACK_REQUIRED)──►  LINK
```

Hex dump of the frame (8 bytes on the wire): `AA 01 01 70 SS 00 CRC_lo CRC_hi`, where `SS` is the next `SEQ`.

**LINK → MCU:**

```
LINK ──UART CommandAck (or no ACK-specific frame, generic ack path)──►  MCU
```

There is no separate ACK type for `ClaimStart`; `UartBridge` uses the `sendCommandAck` path with the same `SEQ`.

LINK state: `cloudState = Provisioning (2)`.

---

## Step 2. Provision - obtain `deviceToken`

```
LINK ──HTTPS POST /devices/provision──►  Portal
      { "serialNumber": "<serial>" }
```

**200 response:**

```json
{
  "deviceToken": "e3b0c44298...",
  "serialNumber": "DEVICE_aabbccddeeff",
  "isNew": true,
  "isClaimed": false
}
```

LINK stores `deviceToken` in NVS.

### If the device is already claimed

```json
{
  "deviceToken": null,
  "isClaimed": true,
  "serialNumber": "DEVICE_aabbccddeeff"
}
```

LINK sends `ClaimStatus` to the MCU with `status = Error (4)`. The MCU shows “the device is already claimed; remove it in the app and try again.” The process ends here in most firmware builds.

---

## Step 3. Register - obtain the PIN

```
LINK ──HTTPS POST /devices/register──►  Portal
      { "token": "e3b0c44298...", "serialNumber": "DEVICE_aabbccddeeff" }
```

**200 response:**

```json
{
  "pin": "12345678",
  "expiresAt": "2026-04-19T12:10:00.000Z",
  "remainingSeconds": 600
}
```

LINK state: `cloudState = Registering (3)` → immediately after receiving the PIN → `AwaitingClaim (4)`.

---

## Step 4. LINK sends the PIN to the MCU

**LINK → MCU:**

```
LINK ──UART ClaimStatus (0x71, LEN=18)──►  MCU
      {
        status: WaitingClaim (2),
        pin: "12345678\0",
        expiresAt: <unix timestamp>,
        remainingSeconds: 600
      }
```

The MCU displays the PIN and remaining time on the screen. The display format is up to you.

LINK can repeat `ClaimStatus` periodically, updating `remainingSeconds` every second or every few seconds depending on the firmware. In the reference Link implementation, this is done by the `onClaimPinUpdate` callback.

---

## Step 5. The user enters the PIN in the app

The app (**not the device**) does this:

```
App ──HTTPS POST /devices/claim──►  Portal
     Authorization: Bearer <user JWT>
     { "pin": "12345678", "name": "My Dryer" }
```

**201 response:**

```json
{
  "deviceId": "3fa85f64-5717-4562-b3fc-2c963f66afa6",
  "serialNumber": "DEVICE_aabbccddeeff",
  "name": "My Dryer",
  "claimed": true
}
```

The device does **not** participate in this step. Everything happens between the app and the portal.

Portal in the database: `Link` moves to `CLAIMED`, and a `Device` is created with the specified name.

---

## Step 6. LINK polls check-claim

In parallel with steps 4-5, LINK does this every 5 seconds:

```
LINK ──HTTPS GET /devices/check-claim/<deviceToken>──►  Portal
```

**Before claim:**

```
404 Not Found
{ "claimed": false }
```

**After claim:**

```json
200 OK
{ "claimed": true, "deviceId": "3fa85f64-5717-4562-b3fc-2c963f66afa6" }
```

### If the PIN expires

If the user does not enter the PIN within 10 minutes:

- LINK can perform another `register` and receive a new PIN.
- Or it can send `ClaimStatus { status = Idle }` and return the MCU menu to normal operation.

---

## Step 7. LINK reports completion to the MCU

**LINK → MCU:**

```
LINK ──UART ClaimComplete (0x72, LEN=38)──►  MCU
      {
        success: 1,
        deviceId: "3fa85f64-5717-4562-b3fc-2c963f66afa6\0"
      }
```

The MCU can briefly show “success” and switch to the main UI. `deviceId` is used only for display; the MCU does not have to store it.

LINK state: `cloudState = Ready (5)`.

---

## Step 8. MQTT connect and publish `info`

```
LINK ──TLS + MQTT CONNECT──►  Broker
      client_id = serialNumber
      username = serialNumber
      password = deviceToken
      LWT = idryer/<serial>/offline

LINK ──SUBSCRIBE idryer/<serial>/commands/#──►  Broker

LINK ──PUBLISH idryer/<serial>/info (retained)──►  Broker
```

LINK state: `cloudState = MqttConnecting (6)` → `Online (7)`.

When the portal receives the first `info` from `Link` in the `CLAIMED` state, it moves the record to **`BOUND`**. The device is fully active.

---

## Full-cycle diagram (condensed)

```
User          MCU          LINK          Portal REST    Broker
(app)
    │              │              │              │             │
    │          [menu: claim]      │              │             │
    │              │  ClaimStart  │              │             │
    │              ├────UART─────►│              │             │
    │              │              │  provision   │             │
    │              │              ├──HTTP───────►│             │
    │              │              │◄────token────┤             │
    │              │              │  register    │             │
    │              │              ├──HTTP───────►│             │
    │              │              │◄────pin──────┤             │
    │              │  ClaimStatus │              │             │
    │              │◄────UART─────┤              │             │
    │              │              │              │             │
    │              │    [displays PIN]          │             │
    │              │              │  check-claim (poll 5s)     │
    │              │              ├──HTTP───────►│             │
    │              │              │◄────404──────┤             │
    │ POST /claim (JWT)           │              │             │
    ├──────────HTTP───────────────┼─────────────►│             │
    │                             │◄────201──────┤             │
    │                             │  check-claim │             │
    │                             ├──HTTP───────►│             │
    │                             │◄──200+uuid───┤             │
    │              │ClaimComplete │              │             │
    │              │◄────UART─────┤              │             │
    │              │              │  MQTT CONNECT               │
    │              │              ├─────────────────────────────►
    │              │              │   SUB commands/#            │
    │              │              ├─────────────────────────────►
    │              │              │   PUB info (retained)       │
    │              │              ├─────────────────────────────►
    │   (device is visible in the app)                         │
    └─────────────────────────────┴──────────────┴─────────────┘
```

---

## Errors and handling

| Error | LINK behavior | UX reaction |
|-------|---------------|-------------|
| No WiFi | does not start provision | MCU shows “no network” |
| 429 (rate limit provision) | retry after 60 s | MCU shows “try again later” |
| `deviceToken: null, isClaimed: true` | sends `ClaimStatus{Error}` | MCU: “remove the device in the app and try again” |
| PIN expired | run `register` again | MCU refreshes the displayed PIN |
| Claim timeout (user did not enter it) | `ClaimComplete{success=0}` | MCU returns to the menu |
| Network error during check-claim | retry with pauses | MCU keeps the PIN on screen |

---

## Recovery after NVS reset

If LINK lost `deviceToken` (board replacement, forced NVS reset):

1. The user initiates claiming again from the same menu.
2. `POST /provision` with the same `serialNumber` returns `deviceToken: null, isClaimed: true` — the portal considers the device already claimed.
3. To continue, the user must unlink the device in the app. Then `Link` returns to `UNCLAIMED`.
4. After unlink, a repeated `provision` gives a new `deviceToken`, and the flow proceeds as on the first claim.

See the portal repository, `LINK_CLAIM_SCENARIOS.md`, for details.

---

## Next steps

- [03-remote-config.md](03-remote-config.md) — menu config exchange between MCU and the cloud.
- [04-profile-mode.md](04-profile-mode.md) — profile drying.
