# Home Assistant

Storage Link は **MQTT Discovery** によって Home Assistant に自身を公開します。HA がエンティティ（SHT31 センサーの温度と湿度、エフェクトの選択、色、LED テープの操作ボタン）を自動的に作成します。ポータルは不要で、すべては自分の MQTT ブローカーを経由します。

以下では、連携の有効化、動作確認、そして本機がエンティティの羅列ではなく整った形で表示されるためのカードレイアウトを説明します。

![Home Assistant の Storage Link カード](../../img/storage-ha-card.png)
*センサーの測定値と LED テープの操作を 1 つのブロックにまとめた状態。*

!!! note
    デバイスは `Settings → Devices & services → Discovered` には**表示されません**。これは UPnP/zeroconf ではなく MQTT Discovery だからです。Home Assistant には **MQTT** 連携をあらかじめ追加しておく必要があります。

## 必要なもの

1. MQTT ブローカー: Home Assistant のアドオン **Mosquitto broker**、またはネットワーク内の任意のブローカー。
2. Home Assistant に、そのブローカーを指す **MQTT** 連携が追加されていること。
3. Storage Link がネットワークに接続され、ポータル上で `Online` であること。

## ステップ 1. 本機で連携を有効にする

[portal.idryer.org](https://portal.idryer.org/) でデバイスを開き、**連携** → **Home Assistant** のブロックを表示します。

| 項目 | 入力する内容 |
|---|---|
| Host | ネットワーク内のブローカーのアドレス。例: `192.168.1.27` |
| Port | ブローカーのポート。通常は `1883` |
| Username / Password | ブローカーが要求する場合の認証情報 |
| Discovery prefix | HA の設定で変更していなければ `homeassistant` |
| 有効 | チェックを入れる。入れないと本機はブローカーに接続しない |

設定はローカルネットワーク経由で本機に直接送信されます。ポータルは保存しません。

![ポータルの「連携」ブロックにある Home Assistant のウィンドウ](../../img/storage-ha-portal-integration.png)
*ブローカーのアドレス、ポート、「有効」のチェック — 本機に必要なのはこれだけです。*

## ステップ 2. Home Assistant でデバイスを探す

`Settings` → `Devices & services` → **MQTT** のカード → **Services** セクションでブローカーのノードを展開します。iDryer の機器は `DEVICE_*` 形式のシリアル番号で表示されます。

![MQTT 連携のページに表示された iDryer の機器](../../img/storage-ha-mqtt-devices.png)
*ブローカーのノード配下にあるデバイス。Storage にはエンティティ数が表示されます。*

デバイスを開くと、HA にはすでに測定値と LED テープの操作要素が表示されています。

## ステップ 3. カードを作成する

HA はエンティティを自動で配置するため、長い一覧になってしまいます。用意されたレイアウトは、測定値を上部に、LED テープの操作を別のブロックとして配置します。

1. `Settings` → `Dashboards` → **Add dashboard** → 空のダッシュボードを作成し、開きます。
2. 右上隅 → 鉛筆アイコン（**Edit**）→ 「⋮」メニュー → **Raw configuration editor**。
3. 以下の内容を貼り付けて保存します。

このレイアウトは `sections` タイプのダッシュボードを前提としています。

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

![レイアウトを貼り付けた Raw configuration editor](../../img/storage-ha-raw-editor.png)
*ダッシュボードの設定エディタに表示された同じレイアウト。*

色は **色** の項目に文字列で指定します。先頭の「#」を付けない 6 桁の HEX 値（例: `FF8800`）です。アニメーションは本機が自ら公開するリストから選択します。古いファームウェアの Storage では選択肢が少なくなります。

## エンティティ名が一致しない場合

このレイアウトは `sensor.storage_temperature` のような標準的な識別子を前提としています。カードに「Entity not found」と表示される場合は、`Settings` → `Devices & services` → **MQTT** → 対象のデバイス → エンティティ一覧で実際の名前を確認し、レイアウト内のプレフィックスを自分のものに置き換えてください。

## トラブルシューティング

| 症状 | 確認する内容 |
|---|---|
| HA にデバイスが表示されない | ポータルの Home Assistant 連携で「有効」にチェックが入っていること、ブローカーのアドレスとポートが正しいこと。本機は `Online` である必要があります。 |
| 表示されたが温度と湿度がない | SHT31 センサーが接続されていないか応答していません。センサーがない場合、本機はこれらのエンティティを公開しません。 |
| ボタンが反応しない | ブローカーで `idryer/#` トピックへの publish が許可されているか、本機のログに認証エラーが出ていないかを確認してください。 |
| 値が `Unknown` のゴーストエンティティ | 以前のファームウェアの retained メッセージが残っています。次のコマンドで消去します: `mosquitto_pub -h <ブローカー> -t 'homeassistant/<...>/config' -n -r`。 |
