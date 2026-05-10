# MQTT: подключение и авторизация

Всё, что нужно, чтобы LINK успешно встал на MQTT-брокер iDryer: параметры соединения, credentials, LWT, подписки.

!!! note "Источник правды"
    `src/mqtt/mqtt_client.cpp` (метод `connect()`), `src/mqtt/idryer_topics.h` (константы), `src/mqtt/root_ca.h` (CA-сертификат). Термины — [../01-overview/04-glossary.md](../01-overview/04-glossary.md).

---

## Брокер

| Параметр | Значение |
|----------|----------|
| Хост продакшен | задаётся конфигурацией окружения (не хранится в репозитории библиотеки) |
| Порт TLS | **8883** |
| Порт plain TCP | **1883** — в продакшене может быть закрыт файрволом |
| Протокол | MQTT 3.1.1 |
| Broker | EMQX |
| Keepalive | **60 секунд** (`IDRYER_MQTT_KEEPALIVE` в `mqtt_client.h`) |
| Clean session | **false** (persistent session на брокере) |
| Max packet size | **16384 байта** (`MQTT_BUFFER_SIZE`) — достаточно для `config` ~3 КБ |
| Auto-reconnect | встроен в `MqttClient::loop()` (через `CloudStateMachine`) |

!!! warning "Clean session: никогда не `true`"
    Устройство обязательно подключается с `clean_session = false` (persistent session, в протоколе MQTT 3.1.1 — флаг `c0`).

    **Почему это критично:** с `clean_session = true` брокер стирает подписку `idryer/<serial>/commands/#` при каждом CONNECT. У устройства тогда один шанс отправить SUBSCRIBE успешно. Если SUBSCRIBE не долетит (лаг WiFi, переполнение буфера PubSubClient, half-closed TCP) — подписки нет, команды от портала молча **не доставляются**, хотя publish от устройства работает. Такое уже было в проде — см. коммит `a75ebc9` в `idryer-protocol`.

    В коде `MqttClient::connect()` стоит флаг `static constexpr bool kMqttCleanSession = false;` с большим предупреждением. Менять запрещено.

    Признак рабочей прошивки в Mosquitto-логе:
    ```
    New client connected ... as <serial> (p2, c0, k60, ...)
                                           ^^^
                                          c0 = clean_session=false
    ```
    Если видите `c1` — значит устройство с багованной прошивкой.

В продакшене ожидается **TLS**. Для локальной разработки и тестов допустим plain 1883. Ключ сборки: `MQTT_USE_TLS`.

### TLS

CA-сертификат — Let's Encrypt root, встроен в прошивку (`src/mqtt/root_ca.h`). Реализация HTTPS — `WiFiClientSecure::setCACert(ROOT_CA_LETSENCRYPT)`. **Не используется** `setInsecure()`.

Если вы переносите прошивку на другую инфраструктуру с другим удостоверяющим центром, замените содержимое `root_ca.h` на ваш корневой сертификат в PEM-формате.

---

## Credentials

| Параметр | Значение | Источник |
|----------|----------|----------|
| Client ID | `serialNumber` | формируется LINK при первом старте (MAC или mcuSerial), хранится в NVS |
| Username | `serialNumber` | совпадает с Client ID |
| Password | `deviceToken` | выдаётся порталом в ответ на `POST /devices/provision`, хранится в NVS |

### Проверка на брокере

EMQX вызывает HTTP auth hook бэкенда (`MqttAuthService`), передаёт пару `(username, password)`. Бэкенд проверяет существование `Link` с таким `serialNumber` и совпадение `Link.token`. Разрешает соединение, если совпало.

!!! warning "Что не работает"
    - **JWT пользователя** — не годится для MQTT устройства. JWT нужен только приложению при вызове `POST /devices/claim`.
    - **Общий broker-пароль** — нет такого. Каждое устройство имеет свою пару `(serial, token)`.
    - **Служебный пользователь `backend` + `MQTT_BACKEND_SECRET`** — только для инфраструктуры (внутренние публикации), не для устройств.

### Если `deviceToken` утерян (стёрт NVS)

Потребуется повторный provision. Если устройство уже привязано к аккаунту, портал вернёт `deviceToken: null` и потребует сначала отвязать устройство в приложении. Сценарий — в [../06-flows/02-claiming-flow.md](../06-flows/02-claiming-flow.md).

---

## LWT (Last Will and Testament)

При подключении LINK передаёт брокеру «завещание» — сообщение, которое брокер опубликует, если соединение оборвётся **неожиданно** (сбой TCP, падение питания).

| Параметр | Значение |
|----------|----------|
| Topic | `idryer/<serialNumber>/offline` |
| QoS | 1 |
| Retain | **false** |
| Payload | `{}` |

Штатное отключение (через `disconnect()`) LWT **не** шлёт — это и позволяет серверу отличить «устройство ушло корректно» от «устройство пропало».

Реализация: см. вызов `mqttClient_.connect(clientId, username, password, lwtTopic, 1, false, "{}")` в `connect()`.

---

## Подписки

После подключения LINK подписывается на один wildcard-топик:

```
idryer/<serialNumber>/commands/#
```

QoS 1. Все входящие команды — `idryer/<serial>/commands/<имя>` — попадают сюда. Парсинг имени команды и тела JSON — в `CommandHandler::handleMqttCommand`. Подробно: [04-backend-to-device.md](04-backend-to-device.md).

---

## Ответы на команды

MQTT-команды от облака **не подтверждаются** отдельным MQTT-сообщением. Это асимметричный канал:

- Пользователь публикует в `commands/...`.
- Устройство обрабатывает и отражает результат в **периодических публикациях** — `status`, `telemetry`, `config/delta`.

Нет топика вроде `commands/ack` или `responses/...`. Бэкенд и приложение должны понять, что команда применена, по изменению `status` / `telemetry`. Для диагностических команд (`ping`) эффекта в MQTT не будет вовсе — только запись в Serial-лог устройства.

## Ограничение размера payload

`MQTT_BUFFER_SIZE = 16384` байта — максимум, который может принять/послать `PubSubClient` в библиотеке. Типичные размеры:

- `info` — 500–1000 байт.
- `telemetry` — до 400 байт.
- `status` — до 600 байт.
- `config` — до ~3 КБ (зависит от наполнения меню).
- `events` — до 500 байт.

Если меню сложное и JSON `config` начнёт превышать ~16 КБ — потребуется увеличить `MQTT_BUFFER_SIZE` в сборке.

---

## Жизненный цикл подключения

```
1. WiFi connected
2. load serialNumber + deviceToken из NVS
3. tcp + TLS handshake с брокером (порт 8883)
4. MQTT CONNECT:
   - Client ID = serialNumber
   - Username = serialNumber
   - Password = deviceToken
   - LWT = idryer/{serial}/offline, QoS 1, retain false, payload "{}"
5. SUBSCRIBE idryer/{serial}/commands/#, QoS 1
6. PUBLISH idryer/{serial}/info (retained), QoS 1 — один раз после connect
7. далее периодически telemetry, status, weights, rfid, events
```

Между шагами могут быть паузы (WiFi моргнул, TCP оборвался). Это нормально: `CloudStateMachine::loop()` автоматически переподключается.

---

## Ключевые API библиотеки

```cpp
idryer::MqttClient mqtt;

// Подключение (delegated из CloudStateMachine, обычно вручную не вызывается)
mqtt.begin(...);
mqtt.setCredentials(serialNumber, deviceToken);
mqtt.connect();

// Публикация
mqtt.publishInfo(hw, fw, workTime, unitsCount, units, mcuSerial, deviceType);
mqtt.publishTelemetry(jsonDoc);
mqtt.publishStatus(jsonDoc);
mqtt.publishWeights(jsonDoc);
mqtt.publishRfid(jsonDoc);
mqtt.publishEvent(jsonDoc);

// Приём команд
mqtt.setCommandCallback(onMqttCommand);
```

Детали методов — `src/mqtt/mqtt_client.h`.

---

## Что дальше

- [02-topics.md](02-topics.md) — таблица всех топиков с QoS и retain.
- [03-device-to-backend.md](03-device-to-backend.md) — JSON, которые устройство публикует.
- [04-backend-to-device.md](04-backend-to-device.md) — JSON команд от облака.
- [05-examples.md](05-examples.md) — готовые `mosquitto_pub`/`mosquitto_sub` для отладки.
