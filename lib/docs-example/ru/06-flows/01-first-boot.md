# Первый запуск: от включения до Online

Сценарий: устройство включено первый раз после сборки на заводе. Нужно дойти до «работает, видно в приложении пользователя, телеметрия идёт». Хронологически.

!!! note
    Это сквозной сценарий. Отдельные детали — в разделах [../03-uart/](../03-uart/), [../04-mqtt/](../04-mqtt/), [../05-cloud/](../05-cloud/).

---

## Участники

- **MCU** — ваш контроллер.
- **LINK** — сетевой модуль.
- **Cloud** — portal (REST + MQTT).
- **Пользователь** — с приложением.

---

## Этапы

### 0. Boot

Оба устройства включаются. Времени загрузки:

- MCU — обычно 100–500 мс.
- LINK (ESP32) — 1–3 секунды до готовности WiFi-стека.

Кто готов первым — зависит от железа. Протокол симметричный, поэтому это не важно.

### 1. UART handshake

**Сценарий A (MCU готов первым):**

```
MCU  ──Hello(role=0x01, units, fw, mcuSerial)──►  LINK
     (LINK может быть ещё не готов — ACK не придёт; MCU повторяет Hello каждые 5 с)
MCU  ──Hello────────────────────────────────────►  LINK  (готов)
MCU  ◄──HelloAck(ip=0, ssid="")──────────────────  LINK  (WiFi ещё не подключён)
```

**Сценарий B (LINK готов первым):**

```
LINK ──Hello(role=0xFF, HelloRequest)────────────►  MCU
MCU  ──Hello(role=0x01, units, fw, mcuSerial)───►  LINK
LINK ──HelloAck──────────────────────────────────►  MCU
```

Детали — [../02-getting-started/02-quickstart-controller.md](../02-getting-started/02-quickstart-controller.md), секция «Кто говорит первым».

На этом этапе LINK знает: версию прошивки MCU, `deviceType`, топологию камер, `mcuSerial`. Эта информация нужна для публикации `info` в MQTT.

### 2. WiFi connect (LINK)

LINK достаёт из NVS credentials WiFi (SSID/password) и подключается. Если их нет — запускает режим AP с captive portal (в референсной прошивке Link реализовано через `ArduinoWifiManager`, на стороне устройства — отдельный UX; за рамками библиотеки).

Пока WiFi не готов, в Heartbeat LINK→MCU поле `cloudState = WifiConnecting (1)`.

Когда WiFi поднят — LINK получает IP и опционально шлёт повторный `HelloAck` с заполненными `ipAddress` + `ssid` (если прикладной код так делает; это не обязательно).

### 3. Provision и register (claiming)

Если `deviceToken` ещё нет в NVS — устройство **непривязано**. Нужно пройти полный claiming.

Пользователь нажимает «начать привязку» в меню MCU. Дальше — [02-claiming-flow.md](02-claiming-flow.md).

Если устройство уже привязано (токен в NVS) — пропускаем, идём на шаг 4.

**cloudState во время этой фазы:** `Provisioning (2)` → `Registering (3)` → `AwaitingClaim (4)` → `Ready (5)`.

### 4. MQTT connect

LINK знает свой `serialNumber` + `deviceToken`. Подключается к MQTT-брокеру.

```
LINK ──TLS handshake────────────────────►  Broker
LINK ──MQTT CONNECT(clientId=serialNumber,
                   username=serialNumber,
                   password=deviceToken,
                   LWT=idryer/<serial>/offline)──►  Broker
LINK ──SUBSCRIBE idryer/<serial>/commands/#──────►  Broker
```

**cloudState:** `MqttConnecting (6)` → `Online (7)`.

Подробно — [../04-mqtt/01-connection.md](../04-mqtt/01-connection.md).

### 5. Публикация `info` (retained)

После `Online` LINK публикует `info` один раз:

```json
{
  "hardwareVersion": "v1.0",
  "firmwareVersion": "1.2.3",
  "workTimeCounter": 360000,
  "unitsCount": 1,
  "mcuSerial": "36B955AB4350FEDC",
  "deviceType": "dryer",
  "units": [ { "unitId": 0, "capabilities": {...}, "scales": [], "rfid": [0] } ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

Retained — значит приложение пользователя, подписавшись на `idryer/<serial>/info`, **сразу** получит последние данные о устройстве, даже если в этот момент устройство не онлайн.

Backend portal: при получении `info` от `Link` в состоянии `CLAIMED` переводит запись в `BOUND`.

### 6. Постоянный режим

Дальше — обычная работа:

| Периодичность | Действие |
|---------------|----------|
| каждые 1 с (active) / 15 с (idle) | MCU → UART Telemetry → LINK → MQTT telemetry |
| по изменению режима | MCU → UART Status → LINK → MQTT status |
| каждые 5 с | Heartbeat в обе стороны (UART) |
| каждые 10 с / при изменении | MCU → UART Weights → LINK → MQTT weights |
| по событию tag_detected / tag_removed | MCU → UART Rfid → LINK → MQTT rfid |
| по событию приложения | MCU → UART Log → LINK → MQTT events |

Плюс:

- LINK слушает MQTT `commands/#` и при входящей команде превращает её в UART-кадр `Command` / `ConfigPush`, шлёт в MCU.
- LINK ежесекундно в loop обслуживает `UartBridge.loop()` и `MqttClient.loop()`.

---

## Таймлайн в секундах (пример)

```
t=0.0    Питание
t=0.3    MCU готов, шлёт Hello
t=1.5    LINK готов, WiFi begin
t=1.6    LINK получил Hello → HelloAck
t=5.0    WiFi подключён, cloudState = Ready
t=5.1    (Если привязано) MQTT connect
t=5.8    MQTT Online, publish info
t=6.0    Публикация первой telemetry
...
t=5s боевой режим
```

При непривязанном устройстве процесс удлиняется на время claiming — может занять от нескольких секунд до минут (зависит от пользователя).

---

## Что проверить на финише

- [ ] `Heartbeat.cloudState == 7 (Online)` приходит MCU каждые 5 секунд.
- [ ] В приложении пользователя видно устройство, `info` retained подгружен.
- [ ] Публикуется `telemetry` с реальными данными.
- [ ] MQTT `commands/drying` приводит к смене `status` на DRYING.
- [ ] При выключении питания устройства в MQTT появляется LWT в `idryer/<serial>/offline`.

---

## Что дальше

- [02-claiming-flow.md](02-claiming-flow.md) — детальный сценарий привязки.
- [03-remote-config.md](03-remote-config.md) — обмен конфигом меню.
- [04-profile-mode.md](04-profile-mode.md) — профильная сушка.
