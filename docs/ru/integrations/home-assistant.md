# Home Assistant

Storage Link публикует себя в Home Assistant через **MQTT Discovery**: HA сам создаёт сущности — температуру и влажность с датчика SHT31, выбор эффекта, цвет и кнопки управления лентой. Портал для этого не нужен, всё идёт через ваш MQTT-брокер.

Ниже — включение интеграции, проверка и готовая раскладка карточки, чтобы прибор выглядел аккуратно, а не списком сущностей.

![Карточка Storage Link в Home Assistant](../../img/storage-ha-card.png)
*Показания датчика и управление лентой одним блоком.*

!!! note
    Устройство **не появится** в `Settings → Devices & services → Discovered`: это MQTT Discovery, а не UPnP/zeroconf. Интеграция **MQTT** в Home Assistant должна быть добавлена заранее.

## Что нужно

1. MQTT-брокер: дополнение **Mosquitto broker** в Home Assistant или любой брокер в вашей сети.
2. В Home Assistant добавлена интеграция **MQTT**, указывающая на этот брокер.
3. Storage Link в сети и `Online` на портале.

## Шаг 1. Включить интеграцию на приборе

Откройте устройство на [portal.idryer.org](https://portal.idryer.org/) и найдите блок **Интеграции** → **Home Assistant**.

| Поле | Что вписать |
|---|---|
| Host | адрес брокера в вашей сети, например `192.168.1.27` |
| Port | порт брокера, обычно `1883` |
| Username / Password | учётные данные брокера, если он их требует |
| Discovery prefix | `homeassistant`, если не меняли его в настройках HA |
| Включено | галочка — иначе прибор к брокеру не подключится |

Настройки уходят прямо на прибор по локальной сети — портал их не хранит.

![Окно Home Assistant в блоке «Интеграции» на портале](../../img/storage-ha-portal-integration.png)
*Адрес брокера, порт и признак «Включено» — всё, что нужно прибору.*

## Шаг 2. Найти устройство в Home Assistant

`Settings` → `Devices & services` → карточка **MQTT** → в разделе **Services** разверните узел брокера. Приборы iDryer видны под серийными номерами вида `DEVICE_*`.

![Приборы iDryer на странице интеграции MQTT](../../img/storage-ha-mqtt-devices.png)
*Устройства под узлом брокера; у Storage видно число сущностей.*

Откройте устройство: HA уже показывает показания и элементы управления лентой.

## Шаг 3. Собрать карточку

HA раскладывает сущности сам, и получается длинный список. Готовая раскладка ставит показания сверху, а управление лентой — отдельным блоком.

1. `Settings` → `Dashboards` → **Add dashboard** → пустой дашборд, откройте его.
2. Правый верхний угол → карандаш (**Edit**) → меню «⋮» → **Raw configuration editor**.
3. Вставьте содержимое ниже и сохраните.

Раскладка рассчитана на дашборд типа `sections`.

```yaml
title: iDryer
views:
- title: Devices
  path: devices
  type: sections
  max_columns: 4
  sections:
  - type: grid
    background: true
    cards:
    - type: heading
      heading: Storage Link
      heading_style: title
      icon: mdi:package-variant-closed
      badges:
      - type: entity
        entity: sensor.storage_mode
        show_icon: false
        show_state: true
        color: primary
    - type: tile
      entity: sensor.storage_temperature
      name: Temperature
      visibility:
      - condition: state
        entity: sensor.storage_temperature
        state_not:
        - unknown
        - unavailable
    - type: tile
      entity: sensor.storage_humidity
      name: Humidity
      visibility:
      - condition: state
        entity: sensor.storage_humidity
        state_not:
        - unknown
        - unavailable
    - type: heading
      heading: LED strip
      heading_style: subtitle
    - type: tile
      entity: select.storage_turn_on_effect
      name: Effect
      features:
      - type: select-options
      features_position: bottom
    - type: tile
      entity: text.storage_turn_on_color
      name: Color
      icon: mdi:palette
    - type: tile
      entity: button.storage_turn_on
      name: Turn on
      icon: mdi:led-strip-variant
      hide_state: true
      tap_action: &id001
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.storage_turn_on
      icon_tap_action: *id001
    - type: tile
      entity: button.storage_turn_off
      name: Turn off
      icon: mdi:led-strip-variant-off
      hide_state: true
      tap_action: &id002
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.storage_turn_off
      icon_tap_action: *id002
```

![Raw configuration editor со вставленной раскладкой](../../img/storage-ha-raw-editor.png)
*Та же раскладка в редакторе конфигурации дашборда.*

Цвет задаётся строкой в поле **Цвет** — шестизначный HEX без решётки, например `FF8800`. Анимация выбирается из списка, который прибор публикует сам: у старых прошивок Storage вариантов меньше.

## Если имена сущностей не совпали

Раскладка рассчитана на стандартные идентификаторы вида `sensor.storage_temperature`. Если карточка показывает «Entity not found», посмотрите свои: `Settings` → `Devices & services` → **MQTT** → ваше устройство → список сущностей, — и замените префикс в раскладке на свой.

## Диагностика

| Симптом | Что проверить |
|---|---|
| Устройство не появилось в HA | В портале у интеграции Home Assistant стоит галочка «Включено», адрес и порт брокера верны. Прибор должен быть `Online`. |
| Появилось, но температуры и влажности нет | Датчик SHT31 не подключён или не отвечает: без датчика прибор эти сущности не публикует. |
| Кнопки не срабатывают | Проверьте, что у брокера разрешена публикация в топики `idryer/#`, а у прибора в логе нет ошибок авторизации. |
| Сущности-призраки со значением `Unknown` | Остались retained-сообщения от прежней прошивки. Очистите: `mosquitto_pub -h <брокер> -t 'homeassistant/<...>/config' -n -r`. |
