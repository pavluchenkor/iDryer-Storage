# Home Assistant

Storage Link 通过 **MQTT Discovery** 把自己发布到 Home Assistant：HA 自行创建实体 —— SHT31 传感器的温度和湿度、效果选择、颜色以及灯带控制按钮。这不需要门户，全部通过您的 MQTT 代理完成。

下面是启用集成、检查以及现成的卡片布局，让设备显示得整齐，而不是一串实体列表。

![Home Assistant 中的 Storage Link 卡片](../../img/ha-card.png)
*传感器读数和灯带控制在同一个区块中。*

!!! note
    设备**不会出现**在 `Settings → Devices & services → Discovered` 中：这是 MQTT Discovery，不是 UPnP/zeroconf。Home Assistant 中必须事先添加 **MQTT** 集成。

## 需要什么

1. MQTT 代理：Home Assistant 中的 **Mosquitto broker** 加载项，或您网络中的任意代理。
2. Home Assistant 中已添加指向该代理的 **MQTT** 集成。
3. Storage Link 已接入网络，并在门户上显示 `Online`。

## 步骤 1. 在设备上启用集成

在 [portal.idryer.org](https://portal.idryer.org/) 上打开设备，找到 **集成** → **Home Assistant** 区块。

| 字段 | 填写内容 |
|---|---|
| Host | 您网络中代理的地址，例如 `192.168.1.27` |
| Port | 代理端口，通常是 `1883` |
| Username / Password | 代理的凭据，如果代理要求的话 |
| Discovery prefix | `homeassistant`，如果没有在 HA 设置中改过 |
| 已启用 | 勾选 —— 否则设备不会连接到代理 |

设置通过局域网直接发送到设备 —— 门户不保存它们。

![门户「集成」区块中的 Home Assistant 窗口](../../img/ha-portal-integration.png)
*代理地址、端口和「已启用」标记 —— 设备需要的全部内容。*

## 步骤 2. 在 Home Assistant 中找到设备

`Settings` → `Devices & services` → **MQTT** 卡片 → 在 **Services** 部分展开代理节点。iDryer 设备以 `DEVICE_*` 形式的序列号显示。

![MQTT 集成页面上的 iDryer 设备](../../img/ha-mqtt-devices.png)
*代理节点下的设备；Storage 会显示实体数量。*

打开设备：HA 已经显示读数和灯带的控制元素。

## 步骤 3. 组装卡片

HA 自行排列实体，结果是一长串列表。现成的布局把读数放在上方，把灯带控制放成单独的区块。

1. `Settings` → `Dashboards` → **Add dashboard** → 空仪表板，打开它。
2. 右上角 → 铅笔（**Edit**）→「⋮」菜单 → **Raw configuration editor**。
3. 粘贴下面的内容并保存。

该布局适用于 `sections` 类型的仪表板。

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

![粘贴了布局的 Raw configuration editor](../../img/ha-raw-editor.png)
*仪表板配置编辑器中的同一布局。*

颜色在 **颜色** 字段中以字符串形式给出 —— 不带井号的六位 HEX，例如 `FF8800`。动画从设备自己发布的列表中选择：旧固件的 Storage 可选项更少。

## 如果实体名称对不上

该布局针对 `sensor.storage_temperature` 这类标准标识符。如果卡片显示「Entity not found」，请查看自己的名称：`Settings` → `Devices & services` → **MQTT** → 您的设备 → 实体列表，然后把布局中的前缀换成您自己的。

## 诊断

| 症状 | 检查内容 |
|---|---|
| 设备没有出现在 HA 中 | 门户上 Home Assistant 集成勾选了「已启用」，代理地址和端口正确。设备必须是 `Online`。 |
| 出现了，但没有温度和湿度 | SHT31 传感器没有接上或没有响应：没有传感器时设备不会发布这些实体。 |
| 按钮没有反应 | 检查代理是否允许发布到 `idryer/#` 主题，以及设备日志中有没有授权错误。 |
| 数值为 `Unknown` 的幽灵实体 | 旧固件残留的 retained 消息。清除：`mosquitto_pub -h <代理> -t 'homeassistant/<...>/config' -n -r`。 |
