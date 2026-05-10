# Portal HTTP API: Technical Contract

The exact contract of the public REST API of the iDryer portal for LINK firmware. Verified against the backend code (`devices.controller.ts`, `devices.service.ts`, `mqtt-auth.service.ts`) and the nginx proxy (`location /api/` to the NestJS root).

!!! note
    The client of this API is LINK (or your own network module). You do not implement this API. Extended scenarios (the unlink/remove matrix) are in the portal repository, `docs/development/LINK_CLAIM_SCENARIOS.md`.

---

## Base URL

| Environment | URL |
|-------------|-----|
| Production | `https://portal.idryer.org/api` |
| Local development (NestJS without nginx) | `http://localhost:3000` (**without** the `/api` prefix) |

All paths below are specified **relative to the base URL**. Example: `POST /devices/provision` in production = `POST https://portal.idryer.org/api/devices/provision`.

**Required header for POST:** `Content-Type: application/json`.

---

## `serialNumber` format

The LINK identifier participates in provision and MQTT (username, topic prefix). Backend-side validation:

- `DEVICE_<mac>` or `DEVICE_<mac>_<suffix>` - typical for ESP32 Link (WiFi MAC + optional suffix).
- Exactly **16 hex characters** (`0-9A-Fa-f`) - an alternative, used when the MCU has its own flash ID (RP2040 `mcuSerial`).

Allowed examples:

```
DEVICE_aabbccddeeff
DEVICE_aabbccddeeff_1234567
36B955AB4350FEDC
```

The string sent to `provision` must **match** the one that LINK uses in the MQTT Client ID / username.

---

## 1. `POST /devices/provision`

Issues a `deviceToken` to the device.

**Auth:** not required.
**Rate limit:** 10 requests / 60 seconds (controller throttle).

**Body:**

```json
{ "serialNumber": "DEVICE_aabbccddeeff_1234567" }
```

### Response `200/201` - new or existing `UNCLAIMED` Link

```json
{
  "deviceToken": "<secret string>",
  "serialNumber": "<same serial>",
  "isNew": true,
  "isClaimed": false
}
```

- `isNew: true` - a new `Link` record was created.
- `isNew: false` - a record with this serial already existed in `UNCLAIMED` -> the **same** `deviceToken` is returned.
- Store the token in NVS. This is the same token that will later be used as the MQTT password.

### Response `200/201` - device already claimed

```json
{
  "deviceToken": null,
  "serialNumber": "<serial>",
  "isNew": false,
  "isClaimed": true
}
```

`null` for `deviceToken` means the `Link` is in `CLAIMED` or `BOUND`, or there is a legacy `Device` record with the same serial. The safe fallback is to require the user to unlink the device in the app first.

For firmware: show the UX "the device is already claimed, remove it in the app and try again" - see `LINK_CLAIM_SCENARIOS.md` in the portal repository.

---

## 2. `POST /devices/register`

Requests a PIN for display to the user.

**Auth:** not required.
**Rate limit:** 10 / 60 s.

**Body:**

```json
{
  "token": "<deviceToken from provision>",
  "serialNumber": "<optional>"
}
```

`serialNumber` is required if no `Link` record has been created for this token yet (first time). In all other cases, `token` is sufficient.

### Response A - PIN issued

```json
{
  "pin": "12345678",
  "expiresAt": "2026-04-19T12:10:00.000Z",
  "remainingSeconds": 600
}
```

- `pin` - 8 digits, string.
- TTL - 10 minutes.
- A **repeat `register` within the TTL** returns the **same PIN** and only updates `lastSeen`. This lets the user recover the PIN without resetting the claim.

### Response B - device already claimed

```json
{
  "alreadyClaimed": true,
  "deviceId": "<dryer uuid>",
  "serialNumber": "DEVICE_..."
}
```

No PIN is issued; LINK can move on to MQTT and/or poll `check-claim`.

### Errors

- `400 Bad Request` - there is no `Link` for `token` and `serialNumber` was not provided. LINK must call `provision` first.

---

## 3. `POST /devices/claim`

Claims the device to the user's account by PIN. **Called by the user's app, not by the device.** The device is not involved in this request.

**Auth:** required, `Authorization: Bearer <user JWT>`.

**Body:**

```json
{
  "pin": "12345678",
  "name": "My Dryer"
}
```

### Response `201` - success

```json
{
  "deviceId": "<uuid>",
  "serialNumber": "<serial>",
  "name": "My Dryer",
  "claimed": true
}
```

Link moves to `CLAIMED`, and a `Device` record in `CLAIMED_PENDING_BIND` is created. The transition to `BOUND` happens after the first `info` in MQTT.

### Typical errors

- `404 Not Found` - the PIN is invalid or does not exist.
- `400 Bad Request` - the PIN expired or the device is already claimed.
- `401 Unauthorized` - missing or invalid user JWT.

---

## 4. `GET /devices/check-claim/:token`

Checks whether the user has finished claiming. LINK polls this endpoint.

**Auth:** not required.
`:token` in the path is the LINK `deviceToken` from NVS.

### Response `404`

```json
{ "claimed": false }
```

The user has not entered the PIN yet (or the PIN expired and no new one was issued).

### Response `200`

```json
{
  "claimed": true,
  "deviceId": "<uuid>"
}
```

The `Link` is in `CLAIMED` or `BOUND` and has `activeDryerId`. LINK can finish the process, send `ClaimComplete` to MCU, and connect to MQTT.

### Polling frequency

Recommended once every 5 seconds (`IDRYER_CLAIM_POLL_INTERVAL_MS` in the LINK firmware configuration). No rate-limit issues have been observed on this endpoint.

---

## 5. `POST /devices/cleanup-expired`

Administrative cleanup of expired `UNCLAIMED` records.

**Auth:** user JWT with the `ADMIN` or `SUPERUSER` role.

**Response:**

```json
{ "deleted": 42 }
```

**Not a public endpoint**; the device does not call it. Mentioned for completeness.

---

## UART relationship (MCU ↔ LINK)

The full UART scenario is described in [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md). Short sequence:

```
MCU → ClaimStart                               → LINK
                                              LINK → POST /provision   → Portal
                                              LINK → POST /register    → Portal
LINK → ClaimStatus(pin, remaining)            → MCU   (MCU shows PIN)
User enters the PIN in the app:
App  → POST /claim (Bearer JWT)               → Portal
                                              LINK: GET /check-claim/:token (polling)
                                              ... 404 ... 404 ... 200 {deviceId}
LINK → ClaimComplete(success, deviceId)       → MCU
                                              LINK → MQTT CONNECT  → Portal
                                              LINK → publish info   → Portal
```

---

## `curl` examples

For debugging.

### provision

```bash
curl -X POST https://portal.idryer.org/api/devices/provision \
  -H 'Content-Type: application/json' \
  -d '{"serialNumber":"DEVICE_aabbccddeeff"}'
```

### register

```bash
curl -X POST https://portal.idryer.org/api/devices/register \
  -H 'Content-Type: application/json' \
  -d '{"token":"<received above>","serialNumber":"DEVICE_aabbccddeeff"}'
```

### check-claim

```bash
curl https://portal.idryer.org/api/devices/check-claim/<deviceToken>
```

### claim (requires a user JWT)

```bash
curl -X POST https://portal.idryer.org/api/devices/claim \
  -H 'Content-Type: application/json' \
  -H "Authorization: Bearer $USER_JWT" \
  -d '{"pin":"12345678","name":"My Dryer"}'
```

---

## What's next

- [03-command-sink.md](03-command-sink.md) - how the device should apply parsed commands across different architectures (UART bridge or standalone).
- [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md) - the full claiming scenario with UART frames.
