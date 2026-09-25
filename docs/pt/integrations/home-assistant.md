# Home Assistant

O Storage Link publica-se no Home Assistant através de **MQTT Discovery**: o HA cria as entidades por si — temperatura e humidade do sensor SHT31, seleção do efeito, cor e botões de controlo da fita. O portal não é necessário para isto, tudo passa pelo seu broker MQTT.

A seguir: como ativar a integração, como verificar e uma disposição de cartão pronta a usar, para que o dispositivo fique com bom aspeto e não como uma lista de entidades.

![Cartão do Storage Link no Home Assistant](../../img/ha-card.png)
*Leituras do sensor e controlo da fita num único bloco.*

!!! note
    O dispositivo **não aparece** em `Settings → Devices & services → Discovered`: isto é MQTT Discovery, não UPnP/zeroconf. A integração **MQTT** no Home Assistant tem de estar adicionada previamente.

## O que é necessário

1. Um broker MQTT: o add-on **Mosquitto broker** no Home Assistant ou qualquer broker na sua rede.
2. No Home Assistant, a integração **MQTT** adicionada e a apontar para esse broker.
3. O Storage Link na rede e `Online` no portal.

## Passo 1. Ativar a integração no dispositivo

Abra o dispositivo em [portal.idryer.org](https://portal.idryer.org/) e encontre o bloco **Integrações** → **Home Assistant**.

| Campo | O que introduzir |
|---|---|
| Host | o endereço do broker na sua rede, por exemplo `192.168.1.27` |
| Port | a porta do broker, normalmente `1883` |
| Username / Password | as credenciais do broker, se este as exigir |
| Discovery prefix | `homeassistant`, se não o tiver alterado nas definições do HA |
| Ativado | a caixa de verificação — caso contrário o dispositivo não se liga ao broker |

As definições vão diretamente para o dispositivo através da rede local — o portal não as guarda.

![Janela do Home Assistant no bloco «Integrações» do portal](../../img/ha-portal-integration.png)
*O endereço do broker, a porta e a marca «Ativado» — tudo o que o dispositivo precisa.*

## Passo 2. Encontrar o dispositivo no Home Assistant

`Settings` → `Devices & services` → cartão **MQTT** → na secção **Services**, expanda o nó do broker. Os dispositivos iDryer aparecem com números de série no formato `DEVICE_*`.

![Dispositivos iDryer na página da integração MQTT](../../img/ha-mqtt-devices.png)
*Dispositivos sob o nó do broker; no Storage vê-se o número de entidades.*

Abra o dispositivo: o HA já mostra as leituras e os elementos de controlo da fita.

## Passo 3. Montar o cartão

O HA dispõe as entidades por si, e o resultado é uma lista longa. A disposição pronta coloca as leituras em cima e o controlo da fita num bloco separado.

1. `Settings` → `Dashboards` → **Add dashboard** → um painel vazio, abra-o.
2. Canto superior direito → o lápis (**Edit**) → menu «⋮» → **Raw configuration editor**.
3. Cole o conteúdo abaixo e guarde.

A disposição destina-se a um painel do tipo `sections`.

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

![Raw configuration editor com a disposição colada](../../img/ha-raw-editor.png)
*A mesma disposição no editor de configuração do painel.*

A cor é indicada como texto no campo **Cor** — HEX de seis dígitos sem cardinal, por exemplo `FF8800`. A animação escolhe-se numa lista que o próprio dispositivo publica: nos firmwares antigos do Storage há menos opções.

## Se os nomes das entidades não coincidirem

A disposição destina-se aos identificadores padrão do tipo `sensor.storage_temperature`. Se o cartão mostrar «Entity not found», consulte os seus: `Settings` → `Devices & services` → **MQTT** → o seu dispositivo → lista de entidades — e substitua o prefixo na disposição pelo seu.

## Diagnóstico

| Sintoma | O que verificar |
|---|---|
| O dispositivo não apareceu no HA | No portal, a integração Home Assistant tem a caixa «Ativado» marcada e o endereço e a porta do broker estão corretos. O dispositivo tem de estar `Online`. |
| Apareceu, mas não há temperatura nem humidade | O sensor SHT31 não está ligado ou não responde: sem o sensor o dispositivo não publica estas entidades. |
| Os botões não funcionam | Verifique que o broker permite a publicação nos tópicos `idryer/#` e que o registo do dispositivo não tem erros de autorização. |
| Entidades fantasma com o valor `Unknown` | Ficaram mensagens retained de um firmware anterior. Limpe-as: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
