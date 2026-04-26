# Claiming: полный сценарий с байтами UART

Привязка устройства к аккаунту пользователя. Шаг за шагом, с UART-кадрами, HTTP-запросами и состоянием на каждой стороне.

!!! note "Контекст"
    Обзор, зачем это и состояния портала — [../05-cloud/01-claiming-overview.md](../05-cloud/01-claiming-overview.md). HTTP-контракт — [../05-cloud/02-http-api.md](../05-cloud/02-http-api.md). UART-кадры `ClaimStart/Status/Complete` — [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

---

## Исходное состояние

- MCU и LINK включены, UART handshake выполнен (см. [01-first-boot.md](01-first-boot.md)).
- LINK подключён к WiFi.
- `deviceToken` в NVS **отсутствует**.
- У пользователя есть аккаунт на portal.idryer.org и он залогинен в приложение.

---

## Шаг 1. Пользователь инициирует привязку

Пользователь в меню MCU выбирает «начать привязку» (конкретный UI — на ваше усмотрение).

**MCU → LINK:**

```
MCU ──UART ClaimStart (0x70, LEN=0, FLAGS=ACK_REQUIRED)──►  LINK
```

Hex-дамп кадра (8 байт на проводе): `AA 01 01 70 SS 00 CRC_lo CRC_hi`, где `SS` — очередной `SEQ`.

**LINK → MCU:**

```
LINK ──UART CommandAck (or no ACK-specific frame, generic ack path)──►  MCU
```

Для `ClaimStart` отдельного типа ACK нет; `UartBridge` использует путь `sendCommandAck` с тем же `SEQ`.

Состояние LINK: `cloudState = Provisioning (2)`.

---

## Шаг 2. Provision — получение `deviceToken`

```
LINK ──HTTPS POST /devices/provision──►  Portal
      { "serialNumber": "<serial>" }
```

**Ответ 200:**

```json
{
  "deviceToken": "e3b0c44298...",
  "serialNumber": "DEVICE_aabbccddeeff",
  "isNew": true,
  "isClaimed": false
}
```

LINK сохраняет `deviceToken` в NVS.

### Если устройство уже привязано

```json
{
  "deviceToken": null,
  "isClaimed": true,
  "serialNumber": "DEVICE_aabbccddeeff"
}
```

LINK отправляет MCU `ClaimStatus` со `status = Error (4)`. MCU показывает «устройство уже привязано; удалите его в приложении и попробуйте снова». На этом процесс заканчивается (в большинстве прошивок).

---

## Шаг 3. Register — получение PIN

```
LINK ──HTTPS POST /devices/register──►  Portal
      { "token": "e3b0c44298...", "serialNumber": "DEVICE_aabbccddeeff" }
```

**Ответ 200:**

```json
{
  "pin": "12345678",
  "expiresAt": "2026-04-19T12:10:00.000Z",
  "remainingSeconds": 600
}
```

Состояние LINK: `cloudState = Registering (3)` → сразу после получения PIN → `AwaitingClaim (4)`.

---

## Шаг 4. LINK шлёт PIN в MCU

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

MCU отображает PIN и остаток времени на экране. Формат отображения — ваш.

LINK может повторять `ClaimStatus` периодически, обновляя `remainingSeconds` (раз в секунду или раз в несколько секунд — зависит от прошивки). В референсной Link это делается колбэком `onClaimPinUpdate`.

---

## Шаг 5. Пользователь вводит PIN в приложение

Приложение (**не устройство**) делает:

```
App ──HTTPS POST /devices/claim──►  Portal
     Authorization: Bearer <user JWT>
     { "pin": "12345678", "name": "My Dryer" }
```

**Ответ 201:**

```json
{
  "deviceId": "3fa85f64-5717-4562-b3fc-2c963f66afa6",
  "serialNumber": "DEVICE_aabbccddeeff",
  "name": "My Dryer",
  "claimed": true
}
```

Устройство в этом этапе **не участвует** — всё происходит между приложением и порталом.

Portal в БД: `Link` переходит в `CLAIMED`, создаётся `Device` с указанным именем.

---

## Шаг 6. LINK опрашивает check-claim

Параллельно с шагом 4–5 LINK каждые 5 секунд делает:

```
LINK ──HTTPS GET /devices/check-claim/<deviceToken>──►  Portal
```

**До claim:**

```
404 Not Found
{ "claimed": false }
```

**После claim:**

```json
200 OK
{ "claimed": true, "deviceId": "3fa85f64-5717-4562-b3fc-2c963f66afa6" }
```

### Если PIN истёк

Если пользователь не ввёл PIN в течение 10 минут:

- LINK может сделать повторный `register` — получит новый PIN.
- Либо отправить `ClaimStatus { status = Idle }` и MCU вернёт меню обычной работы.

---

## Шаг 7. LINK сообщает MCU о завершении

**LINK → MCU:**

```
LINK ──UART ClaimComplete (0x72, LEN=38)──►  MCU
      {
        success: 1,
        deviceId: "3fa85f64-5717-4562-b3fc-2c963f66afa6\0"
      }
```

MCU может показать кратко «успех» и перейти в основной UI. `deviceId` используется только для отображения — MCU не обязан его сохранять.

Состояние LINK: `cloudState = Ready (5)`.

---

## Шаг 8. MQTT connect и publish info

```
LINK ──TLS + MQTT CONNECT──►  Broker
      client_id = serialNumber
      username = serialNumber
      password = deviceToken
      LWT = idryer/<serial>/offline

LINK ──SUBSCRIBE idryer/<serial>/commands/#──►  Broker

LINK ──PUBLISH idryer/<serial>/info (retained)──►  Broker
```

Состояние LINK: `cloudState = MqttConnecting (6)` → `Online (7)`.

Portal при получении первого `info` от `Link` в `CLAIMED` переводит запись в **`BOUND`**. Устройство полностью активно.

---

## Диаграмма полного цикла (сжатая)

```
Пользователь      MCU          LINK          Portal REST    Broker
(приложение)
    │              │              │              │             │
    │          [менu: claim]      │              │             │
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
    │              │    [отображает PIN]         │             │
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
    │   (устройство видно в приложении)                         │
    └─────────────────────────────┴──────────────┴─────────────┘
```

---

## Ошибки и их обработка

| Ошибка | Как ведёт себя LINK | Реакция UX |
|--------|----------------------|------------|
| Нет WiFi | не начинает provision | MCU показывает «нет сети» |
| 429 (rate limit provision) | retry через 60 с | MCU показывает «попробуйте позже» |
| `deviceToken: null, isClaimed: true` | шлёт `ClaimStatus{Error}` | MCU: «удалите устройство в приложении и повторите» |
| PIN истёк | `register` заново | MCU обновляет отображаемый PIN |
| Таймаут claim (пользователь не ввёл) | `ClaimComplete{success=0}` | MCU возвращается к меню |
| Ошибка сети во время check-claim | retry с паузами | MCU держит PIN на экране |

---

## Восстановление после сброса NVS

Если LINK потерял `deviceToken` (замена платы, принудительный сброс NVS):

1. Пользователь инициирует повторный claim (то же меню).
2. `POST /provision` с тем же `serialNumber` вернёт `deviceToken: null, isClaimed: true` — портал считает устройство уже привязанным.
3. Чтобы продолжить — пользователь должен отвязать устройство в приложении (unlink). Тогда `Link` возвращается в `UNCLAIMED`.
4. После unlink повторный `provision` выдаст новый `deviceToken`, и процесс идёт как при первой привязке.

Подробно — репозиторий портала, `LINK_CLAIM_SCENARIOS.md`.

---

## Что дальше

- [03-remote-config.md](03-remote-config.md) — обмен конфигом меню между MCU и облаком.
- [04-profile-mode.md](04-profile-mode.md) — профильная сушка.
