# MQTT команды iHeater Link

Шпаргалка по ручной отправке команд устройству через MQTT. Полный протокол —
в [`lib/idryer-protocol/docs/en/04-mqtt/`](lib/idryer-protocol/docs/en/04-mqtt/).

Все топики устройства — под префиксом `idryer/<serial>/`.

## Старт нагрева

Топик: `idryer/<serial>/commands/drying` (QoS 1).

```json
{
  "unitId": "U1",
  "params": {
    "temperature": 55,
    "duration": 240
  }
}
```

- `temperature` — цель в °C.
- `duration` — минут до авто-остановки; `0` = бесконечно, пока не придёт `stop`.

Пример:
```bash
mosquitto_pub \
  -h mqtt.idryer.org -p 8883 \
  -u <user> -P '<pass>' \
  -t 'idryer/DEVICE_XXXXXXXXXXXX/commands/drying' -q 1 \
  -m '{"unitId":"U1","params":{"temperature":60,"duration":0}}'
```

## Остановка

Топик: `idryer/<serial>/commands/stop` (QoS 1).

```json
{ "unitId": "U1" }
```

## Поведение `duration`

Если `duration > 0`, устройство взводит внутренний deadline и по истечении
времени применяет `Off` автоматически. Точность — до 1 секунды. При `duration=0`
нагрев длится пока не придёт `stop`.

## Важно — конфликт с интеграциями

При активных Moonraker (`moon_en=true`) или Bambu (`bambu_en=true`) их
колбэки каждую секунду **перезаписывают** команду адаптера своим target. В этом
режиме:

- `commands/drying` применяется однократно, затем перекрывается интеграцией.
- По истечении `duration` устройство применит `Off` — но интеграция через
  ≤1 с может снова включить нагрев со своим target.

Для чистой работы `commands/drying` (ручной режим через портал) отключите
Moonraker/Bambu в меню на время сессии.
