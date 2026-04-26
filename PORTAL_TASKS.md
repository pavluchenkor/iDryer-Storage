# iHeater Link — задачи для разработчика портала

## Назначение

Устройство iHeater Link успешно клеймится, публикует `info`, `telemetry`, `integrations/status`, принимает `commands/ping` и штатные команды нагрева. MQTT-контракт со стороны устройства реализован и отлажен. Требуется доработка UI и backend портала, чтобы пользователь мог:

1. вручную включать/выключать нагрев камеры и задавать целевую температуру;
2. настраивать источник автоматического управления — Bambu Lab или Moonraker/Klipper;
3. видеть текущее состояние нагрева и источника данных.

Весь контракт MQTT уже зафиксирован в документации библиотеки `idryer-protocol` (ссылки в конце документа). Новых протокольных команд не требуется.

---

## Что уже работает и НЕ требует изменений

- Claim-flow по HTTP API `/devices/provision → /register → /check-claim` — устройство привязывается штатно.
- Публикация `info`, `telemetry`, `integrations/status` — идёт в стандартные топики `idryer/{serial}/...`.
- Приём `commands/ping` и синхронизация времени — работает.
- Базовая страница устройства на портале с карточкой «Link Module» (Serial, State=Bound, UNLINK MODULE) — остаётся.
- Существующий endpoint «Configure Bambu Lab» — остаётся как отправная точка для пункта 3.

---

## Задача 1. Ручное управление нагревом (UI)

Добавить на страницу устройства блок ручного управления.

### Состав блока

- Поле выбора целевой температуры. Диапазон **45..80 °C**, шаг **1 °C** (числовое поле или слайдер — на усмотрение UI-дизайнера). На стороне прошивки значение при необходимости квантуется.
- Кнопка **Start** — публикует MQTT-сообщение `idryer/{serial}/commands/drying`.
- Кнопка **Stop** — публикует MQTT-сообщение `idryer/{serial}/commands/stop`.

### Payload Start

Топик: `idryer/{serial}/commands/drying`, QoS 1.

```json
{
  "unitId": "U1",
  "params": {
    "temperature": 55,
    "duration": 0
  }
}
```

- `temperature` — целевая температура камеры, целое число в диапазоне 45..80.
- `duration: 0` — работа без автоотключения (до Stop или до перехвата автоматикой).
- `duration: N` (опционально) — автоотключение через N минут.

### Payload Stop

Топик: `idryer/{serial}/commands/stop`, QoS 1.

```json
{ "unitId": "U1" }
```

### Контракт

Полное описание команд `drying` и `stop` — в документе:

`/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/04-backend-to-device.md`

Отдельная секция именно про ручное управление камерой iHeater (`deviceType: link_ii`) — в файле:

`/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/07-portal-integration-contract.md`

---

## Задача 2. Configure Moonraker (UI + backend)

Добавить в карточку устройства кнопку **CONFIGURE MOONRAKER** по аналогии с уже существующей **CONFIGURE BAMBU LAB**.

### Поля формы

- `host` — IP-адрес или hostname Klipper-хоста, например `klipper.local` или `192.168.1.42`.
- `port` — порт Moonraker, по умолчанию `7125`.
- `apiKey` — API-ключ Moonraker, **опциональное поле**. По умолчанию API-ключ в Moonraker не требуется, достаточно прописать доверенную сеть в конфиге Moonraker на стороне Klipper. Поле в UI должно быть помечено как «опционально».
- `enabled` — чекбокс «Включить».

### Backend endpoint

По той же схеме, что уже сделано для Bambu:

`POST /devices/{id}/configure-moonraker`

Payload:

```json
{
  "enabled": true,
  "host": "klipper.local",
  "port": 7125,
  "apiKey": ""
}
```

Backend сохраняет настройку в БД и **обязан транзитом** отправить на устройство MQTT-сообщение `idryer/{serial}/commands/link_integration` с `type: "moonraker"` (подробнее — задача 3).

### Настройка Klipper со стороны пользователя

Чтобы Moonraker отдавал устройству температуру камеры через WebSocket, пользователь должен один раз добавить в `printer.cfg` макрос `VIRTUAL_CHAMBER`. Инструкция для пользователя — в файле:

`/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/virtual_chamber_guide.md`

Рекомендация: добавить ссылку на этот файл (или его краткий пересказ) прямо в help-text формы Configure Moonraker.

---

## Задача 3. Транзит настроек интеграций на устройство (backend)

Это ключевой пункт. Оба эндпоинта **Configure Bambu Lab** и **Configure Moonraker** должны не только сохранять настройки в БД, но и публиковать их на устройство.

### Правило

При успешном `POST /devices/{id}/configure-bambu` и `POST /devices/{id}/configure-moonraker` backend **обязан** опубликовать в топик `idryer/{serial}/commands/link_integration` JSON с дискриминатором `type`. Иначе устройство не узнает о новых настройках и интеграция не поднимется.

### Payload для Bambu

```json
{
  "type": "bambu",
  "enabled": true,
  "ip": "192.168.1.50",
  "serial": "01S00A...",
  "lanAccessCode": "12345678"
}
```

### Payload для Moonraker

```json
{
  "type": "moonraker",
  "enabled": true,
  "host": "klipper.local",
  "port": 7125,
  "apiKey": ""
}
```

### Payload для Home Assistant (уже есть на портале, оставляем)

```json
{
  "type": "ha",
  "enabled": true,
  "host": "homeassistant.local",
  "port": 1883,
  "username": "...",
  "password": "..."
}
```

### Контракт

Полное описание команды `link_integration` (все три варианта payload) — в документе:

`/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/04-backend-to-device.md`

Отдельные страницы по каждой интеграции:

- Home Assistant: `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/04-home-assistant.md`
- Bambu: `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/05-bambu-integration.md`
- Moonraker: `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/06-moonraker-printer.md`

---

## Задача 4. Индикаторы состояния интеграций (UI)

Рядом с каждой кнопкой **CONFIGURE HOME ASSISTANT**, **CONFIGURE BAMBU LAB**, **CONFIGURE MOONRAKER** показать индикатор текущего состояния соответствующей интеграции.

### Источник данных

Устройство публикует в retained-топик `idryer/{serial}/integrations/status` JSON со снимком состояния всех трёх интеграций:

```json
{
  "active": "moonraker",
  "ha": {
    "configured": false,
    "enabled": false,
    "state": "disabled",
    "authUsed": false,
    "lastError": "",
    "updatedAt": "2026-04-20T13:17:00Z"
  },
  "bambu": {
    "configured": false,
    "enabled": false,
    "state": "disabled",
    "lastError": "",
    "updatedAt": "2026-04-20T13:17:00Z"
  },
  "moonraker": {
    "configured": true,
    "enabled": true,
    "state": "connected",
    "virtualChamberAvailable": true,
    "chamberHasSensor": false,
    "chamberTarget": 60,
    "chamberTemperature": 0,
    "lastError": "",
    "updatedAt": "2026-04-20T13:17:00Z"
  },
  "updatedAt": "2026-04-20T13:17:00Z",
  "timestamp": "2026-04-20T13:17:00Z"
}
```

### Правила отображения индикатора

Для каждой из трёх интеграций (`ha`, `bambu`, `moonraker`):

| Состояние | Индикатор | Логика |
|-----------|-----------|--------|
| `configured: false` | серый / off | Интеграция ещё не настроена |
| `configured: true, enabled: false` | серый / off | Настроена, но выключена пользователем |
| `state: "connected"` | зелёный | Клиент подключён к источнику |
| `state: "connecting"` / `"reconnecting"` | жёлтый | В процессе подключения |
| `state: "disabled"` при `configured: true, enabled: true` | жёлтый | Ждёт применения |
| `state: "error"` или `lastError != ""` | красный | Ошибка подключения, можно показать `lastError` в tooltip |

Дополнительно где-нибудь на странице показать какой источник сейчас **активный** (`active`): `none` / `ha` / `bambu` / `moonraker`. Одновременно работает только одна интеграция — её выбирает прошивка по политике приоритетов.

### Backend

Backend **подписан** на `idryer/+/integrations/status` и прокидывает обновления на фронтенд через WebSocket/SSE. Топик retained — после перезагрузки фронтенда актуальное значение всегда доступно.

### Контракт

Подробное описание топика `integrations/status` и правила отображения — в документах:

- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/02-topics.md`
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/07-portal-integration-contract.md`
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/03-link-integrations-overview.md`

---

## Задача 5. Отображение текущего состояния нагрева (UI)

На странице устройства показать актуальное состояние нагрева. Источник данных — retained-топик `idryer/{serial}/telemetry`. Портал берёт последнее полученное значение, специальная частота опроса не требуется.

### Поля для отображения

Из корня JSON:

- `outputMode` — `0` = нагрев выключен, `1` = нагрев включён с целевой температурой.
- `targetTempC` — текущая целевая температура камеры в °C.
- `pulseCode` — код, передаваемый на STM32-контроллер (`10` = OFF, `45..80` = температура). Полезно для диагностики.
- `active` — текущий активный источник команд (`none / ha / bambu / moonraker`).
- `rssi`, `uptime` — справочно.

Вложенные блоки с «сырыми» данными от источников:

- `moonraker.printerState`, `moonraker.chamberTarget`, `moonraker.chamberTemp`, `moonraker.hasSensor`.
- `bambu.gcodeState`, `bambu.chamberTarget`, `bambu.chamberTemp`, `bambu.tray`.

### Пример отображения

- **Нагрев:** OFF / ON @ 60 °C
- **Источник:** Moonraker (`active: moonraker`)
- **Данные Klipper:** `printing`, камера 48 °C → цель 60 °C

Детализация полей — в документе:

`/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/03-device-to-backend.md`

---

## Что НЕ делать

- Не переделывать MQTT pipeline — топики и их структура остаются как есть.
- Не вводить новую сущность устройства в БД — iHeater Link проходит через стандартный `Link + Device` flow.
- Не менять claim / unlink логику — работает штатно.
- Не менять формат `info` / `telemetry` — устройство уже публикует в нужном виде.
- Не публиковать `commands/bambu_apply` на устройства с `deviceType: link_ii` — этот канал только для сушилок (`deviceType: dryer`). Подробности — в файле `07-portal-integration-contract.md` (ссылка выше).

---

## Сводный список документации

Все пути абсолютные. Корень проекта устройства — `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/`.

### Общий план и архитектура устройства

- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/IHEATER_LINK_IMPLEMENTATION_PLAN.md` — план MVP, архитектура, статус компонентов.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/IHEATER_LINK_PULSE_PROTOCOL.md` — pulse-протокол ESP32 → STM32 (к порталу отношения не имеет, справочно).
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/virtual_chamber_guide.md` — настройка Klipper для интеграции с Moonraker.

### MQTT-контракт (библиотека idryer-protocol)

- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/01-connection.md` — подключение, аутентификация, keepalive.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/02-topics.md` — полный список топиков.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/03-device-to-backend.md` — что публикует устройство.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/04-backend-to-device.md` — команды от портала к устройству (`drying`, `stop`, `link_integration` и др.).
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/04-mqtt/05-examples.md` — готовые `mosquitto_pub` для ручной проверки каждой команды.

### HTTP API портала

- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/05-cloud/02-http-api.md` — `/provision`, `/register`, `/claim`, `/check-claim`.

### Интеграции (Bambu / Moonraker / HA)

- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/03-link-integrations-overview.md` — общий обзор LINK-интеграций.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/04-home-assistant.md` — HA.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/05-bambu-integration.md` — Bambu.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/06-moonraker-printer.md` — Moonraker.
- `/Users/ruslanpavlucenko/Projects/iDryerProject/docs/iHeater-link/lib/idryer-protocol/docs/ru/07-features/07-portal-integration-contract.md` — **главный документ для портала**: что публикует устройство, что публикует backend, правила обработки `integrations/status`, поведение по `deviceType`.
