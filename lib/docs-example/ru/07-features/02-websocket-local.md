# WebSocket local access: управление без облака

Альтернативный канал управления устройством **в локальной сети без облака**. Полезен, когда:

- интернет временно недоступен;
- устройство в изолированной локальной сети;
- пользователь хочет локальное управление для безопасности.

Полный аналог MQTT API: те же JSON-команды, тот же список действий. Только транспорт — WebSocket вместо MQTT.

!!! note "UART-коды и детали структур"
    `0x73`–`0x76`. Структуры `WsEnablePayload`, `WsStatusPayload` — [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md).

---

## Архитектура

```
   RP2040 (MCU)                            ESP32 (LINK)
  ┌─────────────────┐                     ┌────────────────────────────────┐
  │ Меню → WsEnable │──UART 0x73─────────►│ WsServer (port 81)             │
  │                 │                     │  ├─ mDNS (_idryer._tcp)         │
  │ Экран ← WsStatus│◄─UART 0x74──────────┤  ├─ WebSocketsServer            │
  │                 │                     │  ├─ PIN auth (4 цифры)          │
  │ Меню → WsReset  │──UART 0x75─────────►│  └─ Client binding (max 5, NVS) │
  │ Меню → WsReq    │──UART 0x76─────────►│                                 │
  └─────────────────┘                     │ CloudStateMachine + MQTT         │
                                           └────────────────────────────────┘
                                                   │           │
                                              WS :81      MQTT :8883
                                                   │           │
                                               ┌───┴───────────┴───┐
                                               │    App / Client   │
                                               └───────────────────┘
```

WS и MQTT могут работать **одновременно**. Команда, пришедшая по WS, применяется так же, как MQTT-команда.

---

## UART-контракт

| Код | Имя | Направление | Payload | Описание |
|-----|-----|-------------|---------|----------|
| `0x73` | `WsEnable` | MCU → LINK | `WsEnablePayload` (4 байта) | Включить/выключить сервер, передать PIN |
| `0x74` | `WsStatus` | LINK → MCU | `WsStatusPayload` (6 байт) | Статус для отображения на экране |
| `0x75` | `WsResetClients` | MCU → LINK | — (пустой) | Сбросить все привязки клиентов |
| `0x76` | `WsStatusRequest` | MCU → LINK | — (пустой) | Запросить текущий статус |

### Типичные сценарии

**Включение WS:**

```
MCU → WsEnable { enable=1, pin=4829 }
LINK: запускает WsServer + mDNS
LINK → WsStatus { state=Listening, pin=4829, paired=0, max=5 }
MCU: показывает PIN в меню как read-only
```

**Подключение клиента:**

```
App → WS connect ws://DEVICE_<serial>.local:81
App → {"type":"auth","pin":"4829","clientId":"app-uuid-xxx"}
LINK → {"type":"auth_ok","deviceName":"DEVICE_<serial>"}
LINK → WsStatus { state=Connected, pin=4829, paired=1, max=5 }
```

**Сброс привязок:**

```
MCU → WsResetClients
LINK: очищает NVS privings, отключает клиентов
LINK → WsStatus { state=Listening, pin=4829, paired=0, max=5 }
```

**Выключение:**

```
MCU → WsEnable { enable=0, pin=0 }
LINK: останавливает WsServer и mDNS
LINK → WsStatus { state=Disabled, pin=0, paired=0, max=5 }
```

---

## WebSocket протокол (ESP32 ↔ App)

### Общее

- Порт: **81**
- Протокол: WebSocket (RFC 6455), текстовые фреймы
- Формат сообщений: JSON
- mDNS: `DEVICE_<MAC>_<random>.local` (имя совпадает с `serialNumber`)
- mDNS service: `_idryer._tcp`, порт 81
- Максимум 5 привязанных клиентов

### PIN

- 4 цифры (1000–9999).
- Генерируется **на MCU** при первом включении WS.
- Хранится в EEPROM MCU (поле `ws_pin` в меню).
- Передаётся в LINK через `WsEnablePayload.pin`.
- LINK не хранит и не генерирует PIN — транспарентно использует значение из MCU.

### Авторизация

**Первое подключение** (нужен PIN):

```json
→  {"type":"auth","pin":"4829","clientId":"app-uuid-xxx"}
←  {"type":"auth_ok","deviceName":"DEVICE_<serial>"}
```

При успехе: LINK сохраняет `clientId` в NVS (список привязанных клиентов).

**Повторное подключение** (PIN не нужен — клиент уже привязан):

```json
→  {"type":"auth","pin":"","clientId":"app-uuid-xxx"}
←  {"type":"auth_ok","deviceName":"DEVICE_<serial>"}
```

Сервер сверяет `clientId` со списком; если привязан — авторизует без PIN.

**Неверный PIN:**

```json
→  {"type":"auth","pin":"9999","clientId":"..."}
←  {"type":"auth_failed","reason":"invalid_pin"}
```

Клиент отключается.

---

## Команды и публикации

После авторизации WS-клиент получает тот же набор команд, что и MQTT. Формат:

```json
→  {"type":"cmd","command":"drying","data":{"unitId":"U1","params":{"temperature":55,"duration":240}}}
```

Сервер направляет `data` в тот же `CommandHandler::handleMqttCommand("drying", data)`, что и MQTT. Никакой разницы в применении.

Публикации от устройства (аналог топиков):

```json
←  {"type":"telemetry","data":{ ... }}
←  {"type":"status","data":{ ... }}
←  {"type":"events","data":{ ... }}
```

Форматы `data` совпадают с MQTT-публикациями — см. [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md).

---

## Жизненный цикл в прошивке

1. MCU включает WS из меню → UART `WsEnable{enable=1, pin=...}`.
2. LINK запускает `WsServer`, регистрирует mDNS.
3. LINK шлёт `WsStatus{Listening}` → MCU отображает PIN.
4. Клиент подключается через mDNS или IP:81, проходит auth.
5. `WsStatus{Connected}` → MCU показывает «подключено».
6. Команды и публикации ходят двунаправленно.
7. При обрыве соединения сервер возвращается в `Listening`.

---

## Состояния WS на стороне LINK

| Значение | Имя | Описание |
|----------|-----|----------|
| 0 | `Disabled` | WS выключен |
| 1 | `Listening` | Запущен, ждёт подключения |
| 2 | `Connected` | Клиент подключён и авторизован |

---

## Когда использовать WS вместо MQTT

- **Нет интернета**, но есть локальный WiFi.
- **Приватность:** данные не уходят в облако.
- **Низкая задержка:** локальный канал быстрее, чем двойной прогон через брокер.
- **Развёртывание офлайн:** заводской тест, демо без настройки облака.

При одновременной работе MQTT + WS: команды применяются одинаково, публикации идут в обоих каналах (как для MQTT, так и в WS). Приложение может видеть свои данные и локально, и через облако.

---

## Что дальше

- [01-rfid.md](01-rfid.md) — RFID-функционал.
- [../04-mqtt/](../04-mqtt/) — та же логика, но через облако.
- [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md) — `WsEnablePayload` / `WsStatusPayload`.
