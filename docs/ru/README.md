# Storage Link — документация продукта

`Storage Link` в этом репозитории — это не модуль UART для сушилки и не оболочка над `idryer-core`.
Текущий продукт — контроллер адресной LED-ленты на ESP32 с двумя внешними задачами:

- подсветить нужную позицию на ленте по команде из облака или локального приложения;
- публиковать температуру и влажность, если на I2C найден датчик SHT31.

Репозиторий описывает именно этот продукт: его команды, конфиг, пины, сборку и внешние контракты.
Документация библиотеки `idryer-core` живёт отдельно в [`lib/idryer-core/docs/ru/`](../../lib/idryer-core/docs/ru/README.md).

## Разделы

| Раздел | Описание |
|--------|----------|
| [guide/README.md](guide/README.md) | Что делает Storage Link, как он собран и где проходит граница продукта |
| [features/led-control.md](features/led-control.md) | Подсветка нужной катушки, `led.pulse`, ограничения и поведение ленты |
| [features/climate-sensor.md](features/climate-sensor.md) | Работа с SHT31, когда появляется телеметрия, что публикуется |
| [protocol/mqtt-commands.md](protocol/mqtt-commands.md) | MQTT-контракт продукта: `commands/invoke`, `commands/set`, `get_config`, `telemetry` |
| [protocol/local-access.md](protocol/local-access.md) | Локальный LAN WebSocket API, авторизация и форматы сообщений |
| [reference/hardware-and-build.md](reference/hardware-and-build.md) | Поддерживаемые платы, пины, env-ы PlatformIO и артефакты сборки |
| [reference/config-menu.md](reference/config-menu.md) | Актуальное меню конфигурации: ID, диапазоны, дефолты, что требует reboot |
| [portal/device-api.md](portal/device-api.md) | Что должен делать портал/приложение для этого продукта и чего продукт пока не умеет |
