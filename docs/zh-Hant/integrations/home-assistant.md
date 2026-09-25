# Home Assistant

Storage Link 透過 **MQTT Discovery** 在 Home Assistant 中發布自己：HA 會自行建立實體——來自 SHT31 感測器的溫度與濕度、效果選擇、顏色，以及燈條控制按鈕。這不需要入口，全部經由您的 MQTT 代理伺服器完成。

以下是啟用整合、檢查，以及現成的卡片版面配置，讓裝置看起來整齊，而不是一串實體清單。

![Home Assistant 中的 Storage Link 卡片](../../img/storage-ha-card.png)
*感測器讀數與燈條控制集中在一個區塊。*

!!! note
    裝置**不會出現**在 `Settings → Devices & services → Discovered` 中：這是 MQTT Discovery，而不是 UPnP/zeroconf。Home Assistant 中的 **MQTT** 整合必須事先新增。

## 需要準備什麼

1. MQTT 代理伺服器：Home Assistant 中的 **Mosquitto broker** 附加元件，或您網路中的任何代理伺服器。
2. Home Assistant 中已新增指向該代理伺服器的 **MQTT** 整合。
3. Storage Link 已連上網路，並在入口中顯示 `Online`。

## 步驟 1. 在裝置上啟用整合

在 [portal.idryer.org](https://portal.idryer.org/) 上開啟裝置，找到 **整合** → **Home Assistant** 區塊。

| 欄位 | 填寫內容 |
|---|---|
| Host | 您網路中代理伺服器的位址，例如 `192.168.1.27` |
| Port | 代理伺服器的連接埠，通常是 `1883` |
| Username / Password | 代理伺服器的認證資訊（如果需要） |
| Discovery prefix | `homeassistant`，如果未在 HA 設定中更改過 |
| 啟用 | 勾選——否則裝置不會連接到代理伺服器 |

設定會透過本機網路直接送到裝置——入口不會保存這些設定。

![入口「整合」區塊中的 Home Assistant 視窗](../../img/storage-ha-portal-integration.png)
*代理伺服器位址、連接埠和「啟用」標記——這就是裝置所需的全部內容。*

## 步驟 2. 在 Home Assistant 中找到裝置

`Settings` → `Devices & services` → **MQTT** 卡片 → 在 **Services** 區段中展開代理伺服器節點。iDryer 裝置以 `DEVICE_*` 形式的序號顯示。

![MQTT 整合頁面上的 iDryer 裝置](../../img/storage-ha-mqtt-devices.png)
*代理伺服器節點下的裝置；Storage 處可看到實體數量。*

開啟裝置：HA 已經顯示讀數與燈條控制元件。

## 步驟 3. 組建卡片

HA 會自行排列實體，結果是一長串清單。現成的版面配置把讀數放在上方，燈條控制單獨成一個區塊。

1. `Settings` → `Dashboards` → **Add dashboard** → 空白儀表板，將其開啟。
2. 右上角 → 鉛筆（**Edit**）→「⋮」選單 → **Raw configuration editor**。
3. 貼上以下內容並儲存。

該版面配置適用於 `sections` 類型的儀表板。

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

![貼上版面配置後的 Raw configuration editor](../../img/storage-ha-raw-editor.png)
*儀表板設定編輯器中的同一份版面配置。*

顏色以字串形式填入 **顏色** 欄位——六位數的 HEX，不含井號，例如 `FF8800`。動畫效果從裝置自行發布的清單中選擇：舊韌體的 Storage 選項較少。

## 如果實體名稱不一致

該版面配置基於 `sensor.storage_temperature` 這類標準識別碼。如果卡片顯示「Entity not found」，請查看您自己的識別碼：`Settings` → `Devices & services` → **MQTT** → 您的裝置 → 實體清單，然後將版面配置中的前綴換成您自己的。

## 診斷

| 症狀 | 檢查內容 |
|---|---|
| 裝置未出現在 HA 中 | 入口中 Home Assistant 整合已勾選「啟用」，代理伺服器的位址與連接埠正確。裝置必須為 `Online`。 |
| 出現了，但沒有溫度與濕度 | SHT31 感測器未連接或無回應：沒有感測器時，裝置不會發布這些實體。 |
| 按鈕沒有反應 | 檢查代理伺服器是否允許發布到 `idryer/#` 主題，以及裝置日誌中是否有授權錯誤。 |
| 數值為 `Unknown` 的幽靈實體 | 舊韌體遺留的 retained 訊息。清除方式：`mosquitto_pub -h <代理伺服器> -t 'homeassistant/<...>/config' -n -r`。 |
