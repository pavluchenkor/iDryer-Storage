# HTTP API портала: технический контракт

Точный контракт публичного REST API портала iDryer для прошивки LINK. Сверен с backend-кодом (`devices.controller.ts`, `devices.service.ts`, `mqtt-auth.service.ts`) и с nginx-прокси (`location /api/` на корень NestJS).

!!! note
    Клиент этого API — LINK (или ваш свой сетевой модуль). Вы не пишете этот API. Расширенные сценарии (матрица unlink/remove) — в репозитории портала, `docs/development/LINK_CLAIM_SCENARIOS.md`.

---

## Базовый URL

| Окружение | URL |
|-----------|-----|
| Продакшен | `https://portal.idryer.org/api` |
| Локальная разработка (NestJS без nginx) | `http://localhost:3000` (**без** префикса `/api`) |

Все пути ниже указаны **относительно базы**. Пример: `POST /devices/provision` на проде = `POST https://portal.idryer.org/api/devices/provision`.

**Обязательный заголовок для POST:** `Content-Type: application/json`.

---

## Формат `serialNumber`

Идентификатор LINK, участвует в provision, MQTT (username, префикс топиков). Валидация на стороне backend:

- `DEVICE_<mac>` или `DEVICE_<mac>_<suffix>` — типично для ESP32 Link (MAC WiFi + опциональный суффикс).
- Ровно **16 hex-символов** (`0-9A-Fa-f`) — альтернатива, используется когда MCU имеет свой flash-ID (RP2040 `mcuSerial`).

Примеры допустимых:

```
DEVICE_aabbccddeeff
DEVICE_aabbccddeeff_1234567
36B955AB4350FEDC
```

Строка, отправляемая в `provision`, должна **совпадать** с той, что LINK кладёт в MQTT Client ID / username.

---

## 1. `POST /devices/provision`

Выдача `deviceToken` устройству.

**Auth:** не требуется.
**Rate limit:** 10 запросов / 60 секунд (throttle на контроллере).

**Тело:**

```json
{ "serialNumber": "DEVICE_aabbccddeeff_1234567" }
```

### Ответ `200/201` — новый или существующий `UNCLAIMED` Link

```json
{
  "deviceToken": "<секретная строка>",
  "serialNumber": "<тот же serial>",
  "isNew": true,
  "isClaimed": false
}
```

- `isNew: true` — создана новая запись `Link`.
- `isNew: false` — запись с таким serial уже была в `UNCLAIMED` → возвращается **тот же** `deviceToken`.
- Токен сохраняйте в NVS. Это тот же токен, который потом пойдёт паролем в MQTT.

### Ответ `200/201` — устройство уже привязано

```json
{
  "deviceToken": null,
  "serialNumber": "<serial>",
  "isNew": false,
  "isClaimed": true
}
```

Token = `null` означает: `Link` в состоянии `CLAIMED` или `BOUND`, либо legacy-запись `Device` с тем же serial. Безопасный фолбэк — требовать от пользователя сначала отвязать устройство в приложении.

Для прошивки: покажите UX «устройство уже привязано, удалите его в приложении и повторите» — см. `LINK_CLAIM_SCENARIOS.md` в репозитории портала.

---

## 2. `POST /devices/register`

Запрос PIN для отображения пользователю.

**Auth:** не требуется.
**Rate limit:** 10 / 60 с.

**Тело:**

```json
{
  "token": "<deviceToken из provision>",
  "serialNumber": "<опционально>"
}
```

`serialNumber` обязателен, если запись `Link` по этому токену ещё не создана (первый раз). В остальных случаях достаточно `token`.

### Ответ A — PIN выдан

```json
{
  "pin": "12345678",
  "expiresAt": "2026-04-19T12:10:00.000Z",
  "remainingSeconds": 600
}
```

- `pin` — 8 цифр, строка.
- TTL — 10 минут.
- **Повторный `register` в пределах TTL** возвращает **тот же PIN**, только обновляет `lastSeen`. Это позволяет пользователю восстановить PIN без сброса привязки.

### Ответ B — устройство уже привязано

```json
{
  "alreadyClaimed": true,
  "deviceId": "<uuid сушилки>",
  "serialNumber": "DEVICE_..."
}
```

PIN не выдаётся; LINK может переходить к MQTT и/или опрашивать `check-claim`.

### Ошибки

- `400 Bad Request` — нет `Link` по `token` и не передан `serialNumber`. LINK должен сделать `provision` сначала.

---

## 3. `POST /devices/claim`

Привязка устройства к аккаунту пользователя по PIN. **Вызывается приложением пользователя, не устройством.** Устройство в этом запросе не участвует.

**Auth:** обязателен, `Authorization: Bearer <user JWT>`.

**Тело:**

```json
{
  "pin": "12345678",
  "name": "Моя сушилка"
}
```

### Ответ `201` — успех

```json
{
  "deviceId": "<uuid>",
  "serialNumber": "<serial>",
  "name": "Моя сушилка",
  "claimed": true
}
```

Link переходит в `CLAIMED`, создаётся запись `Device` в состоянии `CLAIMED_PENDING_BIND`. Переход в `BOUND` — после первого `info` в MQTT.

### Типичные ошибки

- `404 Not Found` — PIN неверный или не существует.
- `400 Bad Request` — PIN истёк или устройство уже привязано.
- `401 Unauthorized` — нет/невалидный пользовательский JWT.

---

## 4. `GET /devices/check-claim/:token`

Проверка, завершил ли пользователь claim. LINK поллит этот endpoint.

**Auth:** не требуется.
`:token` в пути — `deviceToken` из NVS LINK.

### Ответ `404`

```json
{ "claimed": false }
```

Пользователь ещё не ввёл PIN (или PIN истёк и нового не было).

### Ответ `200`

```json
{
  "claimed": true,
  "deviceId": "<uuid>"
}
```

Link в `CLAIMED` или `BOUND` и имеет `activeDryerId`. LINK может завершать процесс, передавать MCU `ClaimComplete`, подключаться к MQTT.

### Частота поллинга

Рекомендуется раз в 5 секунд (`IDRYER_CLAIM_POLL_INTERVAL_MS` в конфигурации прошивки LINK). Превышение rate-limit на этом endpoint не наблюдается.

---

## 5. `POST /devices/cleanup-expired`

Служебная очистка просроченных `UNCLAIMED`-записей.

**Auth:** пользовательский JWT с ролью `ADMIN` или `SUPERUSER`.

**Ответ:**

```json
{ "deleted": 42 }
```

**Не публичный endpoint**; устройство его не вызывает. Упомянут для полноты.

---

## Связь с UART (MCU ↔ LINK)

Полный сценарий по UART — [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md). Короткая связка:

```
MCU → ClaimStart                               → LINK
                                              LINK → POST /provision   → Portal
                                              LINK → POST /register    → Portal
LINK → ClaimStatus(pin, remaining)            → MCU   (MCU показывает PIN)
Пользователь вводит PIN в приложение:
App  → POST /claim (Bearer JWT)               → Portal
                                              LINK: GET /check-claim/:token (поллинг)
                                              ... 404 ... 404 ... 200 {deviceId}
LINK → ClaimComplete(success, deviceId)       → MCU
                                              LINK → MQTT CONNECT  → Portal
                                              LINK → publish info   → Portal
```

---

## Примеры `curl`

Для отладки.

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
  -d '{"token":"<полученный выше>","serialNumber":"DEVICE_aabbccddeeff"}'
```

### check-claim

```bash
curl https://portal.idryer.org/api/devices/check-claim/<deviceToken>
```

### claim (требует пользовательский JWT)

```bash
curl -X POST https://portal.idryer.org/api/devices/claim \
  -H 'Content-Type: application/json' \
  -H "Authorization: Bearer $USER_JWT" \
  -d '{"pin":"12345678","name":"My Dryer"}'
```

---

## Что дальше

- [03-command-sink.md](03-command-sink.md) — как устройству применять разобранные команды при разных архитектурах (UART-мост или standalone).
- [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md) — полный сценарий claiming с UART-кадрами.
