# Home Assistant

O Storage Link se publica no Home Assistant via **MQTT Discovery**: o próprio HA cria as entidades — temperatura e umidade do sensor SHT31, seleção de efeito, cor e botões de controle da fita. O portal não é necessário para isso, tudo passa pelo seu broker MQTT.

Abaixo: ativação da integração, verificação e um layout de cartão pronto, para que o aparelho fique organizado e não como uma lista de entidades.

![Cartão do Storage Link no Home Assistant](../../img/ha-card.png)
*Leituras do sensor e controle da fita em um único bloco.*

!!! note
    O aparelho **não vai aparecer** em `Settings → Devices & services → Discovered`: isto é MQTT Discovery, não UPnP/zeroconf. A integração **MQTT** no Home Assistant deve estar adicionada com antecedência.

## O que é necessário

1. Um broker MQTT: o add-on **Mosquitto broker** no Home Assistant ou qualquer broker na sua rede.
2. A integração **MQTT** adicionada no Home Assistant, apontando para esse broker.
3. O Storage Link na rede e `Online` no portal.

## Passo 1. Ativar a integração no aparelho

Abra o dispositivo em [portal.idryer.org](https://portal.idryer.org/) e encontre o bloco **Integrações** → **Home Assistant**.

| Campo | O que preencher |
|---|---|
| Host | o endereço do broker na sua rede, por exemplo `192.168.1.27` |
| Port | a porta do broker, normalmente `1883` |
| Username / Password | as credenciais do broker, se ele as exigir |
| Discovery prefix | `homeassistant`, se você não o alterou nas configurações do HA |
| Ativado | a caixa de seleção — sem ela o aparelho não se conecta ao broker |

As configurações vão direto para o aparelho pela rede local — o portal não as armazena.

![A janela do Home Assistant no bloco «Integrações» do portal](../../img/ha-portal-integration.png)
*O endereço do broker, a porta e a marca «Ativado» — tudo o que o aparelho precisa.*

## Passo 2. Encontrar o dispositivo no Home Assistant

`Settings` → `Devices & services` → o cartão **MQTT** → na seção **Services** expanda o nó do broker. Os aparelhos iDryer aparecem sob números de série no formato `DEVICE_*`.

![Aparelhos iDryer na página da integração MQTT](../../img/ha-mqtt-devices.png)
*Dispositivos sob o nó do broker; no Storage aparece a quantidade de entidades.*

Abra o dispositivo: o HA já mostra as leituras e os controles da fita.

## Passo 3. Montar o cartão

O HA distribui as entidades por conta própria, e o resultado é uma lista longa. O layout pronto coloca as leituras em cima e o controle da fita em um bloco separado.

1. `Settings` → `Dashboards` → **Add dashboard** → um painel vazio, abra-o.
2. Canto superior direito → o lápis (**Edit**) → o menu «⋮» → **Raw configuration editor**.
3. Cole o conteúdo abaixo e salve.

O layout é feito para um painel do tipo `sections`.

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

![Raw configuration editor com o layout colado](../../img/ha-raw-editor.png)
*O mesmo layout no editor de configuração do painel.*

A cor é definida por uma string no campo **Cor** — um HEX de seis dígitos sem o sinal de cerquilha, por exemplo `FF8800`. A animação é escolhida na lista que o próprio aparelho publica: nos firmwares antigos do Storage há menos opções.

## Se os nomes das entidades não coincidirem

O layout é feito para os identificadores padrão do tipo `sensor.storage_temperature`. Se o cartão mostrar «Entity not found», veja os seus: `Settings` → `Devices & services` → **MQTT** → o seu dispositivo → a lista de entidades, — e substitua o prefixo no layout pelo seu.

## Diagnóstico

| Sintoma | O que verificar |
|---|---|
| O dispositivo não apareceu no HA | No portal, a integração Home Assistant está com a marca «Ativado», o endereço e a porta do broker estão corretos. O aparelho deve estar `Online`. |
| Apareceu, mas não há temperatura nem umidade | O sensor SHT31 não está conectado ou não responde: sem o sensor o aparelho não publica essas entidades. |
| Os botões não funcionam | Verifique se o broker permite a publicação nos tópicos `idryer/#` e se não há erros de autorização no log do aparelho. |
| Entidades-fantasma com o valor `Unknown` | Restaram mensagens retained do firmware anterior. Limpe-as: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
