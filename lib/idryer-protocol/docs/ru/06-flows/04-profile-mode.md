# Профильная сушка: от MQTT-команды до PID

Полный сценарий профильного режима: пользователь в приложении задаёт последовательность этапов, команда приходит на устройство, MCU применяет её с логикой ramp/hold, отслеживает прогресс, сообщает о переходах в `status`.

---

## Что такое профиль

Профиль — это **последовательность температурных этапов** с контролем скорости подъёма. Каждый этап задан тремя числами:

- **`temp`** — целевая температура °C.
- **`ramp`** — время в секундах, за которое MCU должен плавно поднять/опустить температуру до `temp`.
- **`hold`** — время в секундах, сколько держать `temp` после достижения.

До 10 этапов. Используется для «сложной» сушки: плавный прогрев, выдержка, интенсивный режим, охлаждение.

---

## Команда запуска

**Topic:** `idryer/<serial>/commands/profile`, QoS 1.

```json
{
  "unitId": "U1",
  "mode": "PROFILE",
  "startStage": 0,
  "stages": [
    { "temp": 60,  "ramp": 300,  "hold": 1800 },
    { "temp": 100, "ramp": 600,  "hold": 6000 },
    { "temp": 70,  "ramp": 600,  "hold": 12000 }
  ],
  "timestamp": "2026-04-19T12:00:00Z"
}
```

Обязательные поля: `unitId`, `stages`. Формат каждого этапа — `temp`/`ramp`/`hold`. Детали — [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md).

---

## UART-транспорт

`CommandHandler` собирает `ProfilePayload` (64 байта) и вызывает `sink->sendProfileCommand(payload, true)`. `UartCommandSink` → `UartBridge::sendProfileCommand` → кадр `MessageKind::Command (0x20)` с **payload 64 байта**.

```
 LINK                                      MCU
  │                                         │
  │ ──Command (0x20, LEN=64, ProfilePayload)──►
  │                                         │
  │                               MCU различает по LEN:
  │                               13 → CommandPayload
  │                               64 → ProfilePayload  ← наш случай
  │                                         │
  │ ◄──CommandAck(seq, OK)──────────────────
  │                                         │
```

MCU должен проверять именно `payloadLength == 64`, а не поля payload-а.

Точный layout байтов — [../03-uart/04-binary-structures.md#profilepayload-внутри-command-0x20--64-байта](../03-uart/04-binary-structures.md). Hex-дамп рабочего кадра — [../03-uart/06-examples.md](../03-uart/06-examples.md) (раздел «Command с ProfilePayload»).

---

## Логика на MCU

Нагрев управляется PID-контуром с изменяющейся целевой температурой.

### Переменные состояния

```c
struct ProfileState {
    uint8_t  currentStage;      // Текущий этап (0-based)
    uint8_t  totalStages;       // Всего этапов
    float    prevTemp;          // Температура на начало этапа
    uint32_t stageStartTime;    // millis() на начало этапа
    uint32_t holdStartTime;     // millis() на начало HOLD-фазы
    bool     inRamp;            // true = RAMP, false = HOLD
};
```

### Автомат этапа

```
┌──────────────────────────────────────────────────────────────┐
│                       PROFILE MODE                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌─────────┐    достигли temp    ┌─────────┐              │
│   │  RAMP   │ ─────────────────►  │  HOLD   │              │
│   │         │    или истёк ramp   │         │              │
│   └────┬────┘                     └────┬────┘              │
│        │ stageTime >= ramp             │ holdTime >= hold   │
│        │   (если ramp > 0)             │                    │
│        ▼                               ▼                    │
│    переход в HOLD              следующий этап               │
│                                или завершение               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Псевдокод основного tick

```c
void profileTick(uint32_t now) {
    Stage *stage = &stages[currentStage];
    uint32_t stageTime = now - stageStartTime;

    if (inRamp) {
        if (stage->ramp == 0) {
            // Форсированный нагрев: греем на максимуме
            targetTemp = stage->temp;
            heaterPower = MAX_POWER;

            // Переход в HOLD, когда достигли цели (с учётом гистерезиса)
            if (currentTemp >= stage->temp - HYSTERESIS) {
                inRamp = false;
                holdStartTime = now;
            }
        } else {
            // Линейная рампа: target растёт по времени
            if (stageTime < stage->ramp * 1000) {
                float progress = (float)stageTime / (stage->ramp * 1000);
                targetTemp = prevTemp + (stage->temp - prevTemp) * progress;
            } else {
                // Рампа закончилась
                targetTemp = stage->temp;
                inRamp = false;
                holdStartTime = now;
            }
        }
    } else {
        // HOLD: поддерживаем целевую температуру
        targetTemp = stage->temp;
        uint32_t holdTime = now - holdStartTime;

        if (holdTime >= stage->hold * 1000) {
            // Этап закончен
            if (currentStage + 1 < totalStages) {
                prevTemp = currentTemp;
                currentStage++;
                stageStartTime = now;
                inRamp = true;
            } else {
                // Профиль завершён
                setMode(IDLE);
                return;
            }
        }
    }

    heaterPower = pidCompute(targetTemp, currentTemp);
}
```

### Особый случай: `ramp = 0`

«Форсированный нагрев»: MCU греет на максимальной мощности до достижения `temp`. Отсчёт `hold` начинается **после** достижения цели, а не с начала этапа. Полезно, когда важно «30 минут при 60 °C», а не «30 минут включая разгон».

---

## Публикация прогресса

Пока профиль работает, `status` обновляется **при каждой смене этапа** и при переходах `RAMP ↔ HOLD`. Для дополнительной наглядности MCU может публиковать `status` чаще (например, каждые 10 секунд).

Поля `status` при PROFILE — см. [../04-mqtt/03-device-to-backend.md](../04-mqtt/03-device-to-backend.md):

- `currentStage` (0-based)
- `totalStages`
- `stageElapsed` / `stageRemaining`
- `stagePhase` — `"RAMP"` или `"HOLD"`
- `totalElapsed` / `totalRemaining` — по всей программе

Пример (1 час в середине 2-го этапа):

```json
{
  "units": [{
    "unitId": "U1",
    "mode": "PROFILE",
    "sessionNum": 42,
    "target": { "temperature": 100.0, "duration": 0 },
    "totalElapsed": 4200,
    "totalRemaining": 10200,
    "currentStage": 1,
    "totalStages": 3,
    "stageElapsed": 600,
    "stageRemaining": 5400,
    "stagePhase": "HOLD"
  }],
  "uptime": 12345
}
```

---

## Прерывание

Команда `stop`:

```json
{ "unitId": "U1" }
```

прерывает профиль немедленно: MCU переходит в Idle, нагреватель выключается. `status` меняется на `"IDLE"` (короткая форма).

---

## Примеры профилей

### Стандартная сушка PLA

```json
{
  "unitId": "U1",
  "stages": [
    { "temp": 50, "ramp": 0, "hold": 14400 }
  ]
}
```

Быстрый нагрев до 50 °C, держать 4 часа.

### Интенсивная сушка ABS

```json
{
  "unitId": "U1",
  "stages": [
    { "temp": 65, "ramp": 600,  "hold": 3600 },
    { "temp": 80, "ramp": 300,  "hold": 7200 },
    { "temp": 65, "ramp": 600,  "hold": 3600 }
  ]
}
```

10 мин прогрев → 1 час при 65 °C → 5 мин быстрый подъём → 2 часа при 80 °C → 10 мин спуск → 1 час при 65 °C.

### Щадящая сушка нейлона

```json
{
  "unitId": "U1",
  "stages": [
    { "temp": 40, "ramp": 1200, "hold": 7200 },
    { "temp": 70, "ramp": 1800, "hold": 28800 },
    { "temp": 50, "ramp": 1200, "hold": 3600 }
  ]
}
```

Очень плавный прогрев во избежание термошока.

---

## Что дальше

- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) — формат JSON команды profile.
- [../03-uart/04-binary-structures.md](../03-uart/04-binary-structures.md) — байт-в-байт ProfilePayload.
- [../03-uart/06-examples.md](../03-uart/06-examples.md) — hex-дамп реального кадра.
