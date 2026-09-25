# Home Assistant

Storage Link se publica en Home Assistant mediante **MQTT Discovery**: HA crea las entidades por sí mismo — temperatura y humedad del sensor SHT31, selección de efecto, color y botones de control de la tira. Para esto no hace falta el portal, todo pasa por tu broker MQTT.

A continuación: activación de la integración, comprobación y una disposición de tarjeta lista para usar, para que el dispositivo se vea ordenado y no como una lista de entidades.

![Tarjeta de Storage Link en Home Assistant](../../img/storage-ha-card.png)
*Lecturas del sensor y control de la tira en un solo bloque.*

!!! note
    El dispositivo **no aparecerá** en `Settings → Devices & services → Discovered`: esto es MQTT Discovery, no UPnP/zeroconf. La integración **MQTT** en Home Assistant debe estar añadida de antemano.

## Qué se necesita

1. Un broker MQTT: el complemento **Mosquitto broker** en Home Assistant o cualquier broker de tu red.
2. En Home Assistant, la integración **MQTT** añadida y apuntando a ese broker.
3. Storage Link en la red y `Online` en el portal.

## Paso 1. Activar la integración en el dispositivo

Abre el dispositivo en [portal.idryer.org](https://portal.idryer.org/) y busca el bloque **Integraciones** → **Home Assistant**.

| Campo | Qué escribir |
|---|---|
| Host | dirección del broker en tu red, por ejemplo `192.168.1.27` |
| Port | puerto del broker, normalmente `1883` |
| Username / Password | credenciales del broker, si las requiere |
| Discovery prefix | `homeassistant`, si no lo has cambiado en los ajustes de HA |
| Activado | la casilla — de lo contrario el dispositivo no se conectará al broker |

Los ajustes van directamente al dispositivo por la red local — el portal no los guarda.

![Ventana de Home Assistant en el bloque «Integraciones» del portal](../../img/storage-ha-portal-integration.png)
*Dirección del broker, puerto y la marca «Activado» — todo lo que el dispositivo necesita.*

## Paso 2. Encontrar el dispositivo en Home Assistant

`Settings` → `Devices & services` → tarjeta **MQTT** → en la sección **Services** despliega el nodo del broker. Los dispositivos iDryer se ven bajo números de serie del tipo `DEVICE_*`.

![Dispositivos iDryer en la página de la integración MQTT](../../img/storage-ha-mqtt-devices.png)
*Dispositivos bajo el nodo del broker; en el Storage se ve el número de entidades.*

Abre el dispositivo: HA ya muestra las lecturas y los elementos de control de la tira.

## Paso 3. Montar la tarjeta

HA dispone las entidades por su cuenta y sale una lista larga. La disposición lista para usar pone las lecturas arriba y el control de la tira en un bloque aparte.

1. `Settings` → `Dashboards` → **Add dashboard** → un dashboard vacío, ábrelo.
2. Esquina superior derecha → el lápiz (**Edit**) → menú «⋮» → **Raw configuration editor**.
3. Pega el contenido de abajo y guarda.

La disposición está pensada para un dashboard de tipo `sections`.

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

![Raw configuration editor con la disposición pegada](../../img/storage-ha-raw-editor.png)
*La misma disposición en el editor de configuración del dashboard.*

El color se indica como texto en el campo **Color** — un HEX de seis dígitos sin almohadilla, por ejemplo `FF8800`. La animación se elige de la lista que el propio dispositivo publica: en los firmware antiguos del Storage hay menos opciones.

## Si los nombres de las entidades no coinciden

La disposición está pensada para identificadores estándar del tipo `sensor.storage_temperature`. Si la tarjeta muestra «Entity not found», mira los tuyos: `Settings` → `Devices & services` → **MQTT** → tu dispositivo → lista de entidades, — y sustituye el prefijo de la disposición por el tuyo.

## Diagnóstico

| Síntoma | Qué comprobar |
|---|---|
| El dispositivo no aparece en HA | En el portal, la integración Home Assistant tiene marcada la casilla «Activado» y la dirección y el puerto del broker son correctos. El dispositivo debe estar `Online`. |
| Aparece, pero no hay temperatura ni humedad | El sensor SHT31 no está conectado o no responde: sin sensor el dispositivo no publica esas entidades. |
| Los botones no funcionan | Comprueba que el broker permita la publicación en los tópicos `idryer/#` y que en el registro del dispositivo no haya errores de autorización. |
| Entidades fantasma con valor `Unknown` | Quedan mensajes retained del firmware anterior. Límpialos: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
