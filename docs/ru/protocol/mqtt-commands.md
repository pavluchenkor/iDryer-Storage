# MQTT-команды Storage Link

Все MQTT-топики продукта живут под префиксом `idryer/<serial>/`.
Для `Storage Link` в этом репозитории актуальны только команды подсветки и конфигурации ленты.

## Что продукт реально публикует

### `info`

Топик: `idryer/<serial>/info`

Публикуется при выходе в `Online` и повторно после `commands/ping`.
Форма соответствует [`LedStripProfile::buildInfoJson()`](../../../src/storage/led_strip/led_strip_profile.cpp).

Пример:

```json
{
  "hardwareVersion": "1.0",
  "firmwareVersion": "1.0.0",
  "deviceType": "storage_link",
  "timestamp": "2026-04-28T12:00:00Z"
}
```

### `telemetry`

Топик: `idryer/<serial>/telemetry`

Публикуется только если:

- устройство подключено к MQTT;
- датчик SHT31 найден на старте;
- чтение датчика валидно.

Важный нюанс по контракту:

- продуктовый код формирует только `units[]`;
- top-level `timestamp` для MQTT добавляется транспортным слоем в `MqttClient::publishJson()`;
- в локальном WebSocket-сообщении этот `timestamp` сейчас не инжектится автоматически.

Пример:

```json
{
  "units": [
    {
      "unitId": "U1",
      "temperature": 23.5,
      "humidity": 47.2
    }
  ],
  "timestamp": "2026-04-28T12:00:10Z"
}
```

### `config`

Топик: `idryer/<serial>/config`

Публикуется по запросу `get_config` или `device.getConfig`.
Содержит меню конфигурации ленты.

## Какие команды принимает продукт

### `commands/invoke`

Топик: `idryer/<serial>/commands/invoke`

Поддерживаемые `action`:

- `led.pulse`
- `led.animation`
- `device.getConfig`

#### `led.pulse`

Назначение: подсветить нужную катушку или позицию на ленте.

```json
{
  "action": "led.pulse",
  "args": {
    "ledIndex": 12,
    "durationSec": 15,
    "color": "#00FFAA"
  }
}
```

- `ledIndex` — индекс светодиода, с нуля.
- `durationSec` — длительность подсветки в секундах.
- `color` — RGB-цвет в формате `#RRGGBB`.

Фактическое поведение:

- если индекс вне диапазона `0..led_count-1`, команда логируется и игнорируется;
- одновременно активен только один светодиод;
- новая команда сначала гасит предыдущий активный светодиод;
- `durationSec = 0` выключает текущую подсветку.

#### `led.animation`

```json
{
  "action": "led.animation",
  "args": {}
}
```

Маршрут уже есть, но в текущем коде это заглушка.
В `LedStripExecutor::handleAnimation()` нет продуктовой реализации, только лог `"not implemented in v1"`.

#### `device.getConfig`

```json
{
  "action": "device.getConfig"
}
```

Возвращает полный `config` без изменения состояния устройства.

### `commands/get_config`

Топик: `idryer/<serial>/commands/get_config`

Пустой payload допустим:

```json
{}
```

Это второй способ получить тот же `config`, что и через `device.getConfig`.

### `commands/set`

Топик: `idryer/<serial>/commands/set`

Payload:

```json
{
  "id": 2,
  "val": 180
}
```

Поддерживаются только ID меню `Storage Link`.
Актуальная таблица приведена в [reference/config-menu.md](../reference/config-menu.md).

На практике используются:

- `2` — `led_count`
- `3` — `psu_ma`
- `5` — `strip_ws2812b`
- `6` — `strip_apa102`
- `8` — `order_grb`
- `9` — `order_rgb`
- `10` — `order_brg`
- `11` — `order_bgr`

Особенности применения:

- `led_count` и `psu_ma` применяются сразу;
- тип ленты и порядок цветов сохраняются сразу, но начинают действовать только после reboot;
- деактивация toggle-пункта (`val = 0`) для групп `strip_*` и `order_*` игнорируется, потому что в группе всегда должен остаться ровно один активный вариант.

### `commands/ping`

Топик: `idryer/<serial>/commands/ping`

Эта команда обрабатывается runtime-слоем.
Продукт использует её только для синхронизации времени и повторной публикации `info`.

## Что продукт сейчас не использует

В этом репозитории `Storage Link` не публикует и не обрабатывает продуктовую логику для:

- `commands/drying`
- `commands/storage`
- `commands/stop`
- `status`
- `events`
- `integrations/status`
- `config/delta`

Эти сущности существуют в платформенном MQTT-контракте, но не являются активной product behavior текущего `Storage Link`.
