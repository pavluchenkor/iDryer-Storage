# Local Access — LAN WebSocket

`Storage Link` поднимает локальный WebSocket-сервер на порту `81`.
Он нужен не как отдельный продуктовый API, а как LAN-зеркало основных команд устройства.

Исходная реализация:

- [`src/main.cpp`](../../../src/main.cpp)
- [`lib/idryer-core/src/local_access/local_access.cpp`](../../../lib/idryer-core/src/local_access/local_access.cpp)

## Что происходит при старте

- mDNS поднимается с именем `<serial>.local`;
- публикуется сервис `_idryer._tcp` на порту `81`;
- WebSocket запускается даже если устройство ещё не claimed, но авторизация не пройдёт без валидного `device token`.

## Ограничения

- одновременно поддерживается только один подключённый клиент;
- без успешной авторизации любые команды отклоняются;
- при неверном токене устройство просит обновить токен из credential store.

## Авторизация

Первое сообщение клиента должно быть таким:

```json
{
  "type": "auth",
  "token": "<device_token>"
}
```

Успешный ответ:

```json
{
  "type": "auth_ok",
  "deviceName": "DEVICE_XXXXXXXXXXXX"
}
```

Неуспешный ответ:

```json
{
  "type": "auth_fail",
  "reason": "invalid_token"
}
```

Сразу после `auth_ok` устройство автоматически отправляет текущий `config`.

## Формат входящих команд

После авторизации клиент шлёт ту же командную модель, что и MQTT, но в WS-конверте:

```json
{
  "type": "command",
  "command": "invoke",
  "data": {
    "action": "led.pulse",
    "args": {
      "ledIndex": 7,
      "durationSec": 10,
      "color": "#FFFFFF"
    }
  }
}
```

Поддерживаемые `command`:

- `invoke`
- `set`
- `get_config`

Маршрутизация общая с MQTT:

- `get_config` перехватывается до dispatcher и возвращает `config`;
- `invoke` уходит в `LedStripExecutor`;
- `set` уходит в `LedStripProfile::applyConfig`.

## Что устройство отправляет обратно

### `config`

Отправляется:

- сразу после успешного `auth`;
- при явном `get_config`;
- при `invoke` с `action = "device.getConfig"`.

Формат:

```json
{
  "type": "config",
  "data": {
    "v": 1,
    "units": 1,
    "active": 0,
    "lang": "en",
    "deviceType": "storage_link",
    "menu": []
  }
}
```

### `telemetry`

Отправляется при тех же условиях, что и MQTT-телеметрия:

- датчик найден;
- есть валидное чтение;
- истёк интервал публикации.

Пример:

```json
{
  "type": "telemetry",
  "data": {
    "units": [
      {
        "unitId": "U1",
        "temperature": 23.5,
        "humidity": 47.2
      }
    ]
  }
}
```

В текущей реализации WS получает сырой `JsonDocument` до MQTT-публикации.
Поэтому top-level `timestamp`, который на MQTT добавляет `MqttClient::publishJson()`, в WS-сообщении `telemetry` сейчас отсутствует.

## Что реально подтверждено для текущего продукта

На уровне общего publish path `DevicePublisher` умеет зеркалировать в WS:

- `info`
- `telemetry`
- `status`
- `config`
- `config/delta`
- `events`
- `integrations/status`

Но для текущего `Storage Link` важно различать две вещи:

- что поддерживает общий транспортный слой;
- что реально вызывается продуктовым кодом этого репозитория.

Подтверждённые product-level сценарии на текущем коде:

- `config` через `s_pub.publishConfig(doc)` в обработчике `get_config`;
- `telemetry` через `StorageTelemetryPublisher`.

`info` сейчас уходит в MQTT напрямую через `IdryerRuntime` и не проходит через `DevicePublisher`, поэтому в LAN-канал не зеркалируется.
Остальные типы (`status`, `events`, `integrations/status`, `config/delta`) транспортный слой поддерживает, но продуктовый код `Storage Link` в этом репозитории их сейчас не публикует.
