# Home Assistant

Storage Link se do Home Assistant publikuje přes **MQTT Discovery**: HA sám vytvoří entity — teplotu a vlhkost ze snímače SHT31, výběr efektu, barvu a tlačítka ovládání pásku. Portál k tomu není potřeba, vše jde přes váš MQTT broker.

Níže je zapnutí integrace, kontrola a hotové rozložení karty, aby zařízení vypadalo úhledně, a ne jako seznam entit.

![Karta Storage Link v Home Assistant](../../img/storage-ha-card.png)
*Hodnoty ze snímače a ovládání pásku v jednom bloku.*

!!! note
    Zařízení se **neobjeví** v `Settings → Devices & services → Discovered`: jde o MQTT Discovery, ne o UPnP/zeroconf. Integrace **MQTT** musí být v Home Assistant přidána předem.

## Co je potřeba

1. MQTT broker: doplněk **Mosquitto broker** v Home Assistant nebo libovolný broker ve vaší síti.
2. V Home Assistant je přidaná integrace **MQTT**, která ukazuje na tento broker.
3. Storage Link je v síti a na portálu `Online`.

## Krok 1. Zapnout integraci na zařízení

Otevřete zařízení na [portal.idryer.org](https://portal.idryer.org/) a najděte blok **Integrace** → **Home Assistant**.

| Pole | Co vyplnit |
|---|---|
| Host | adresa brokeru ve vaší síti, například `192.168.1.27` |
| Port | port brokeru, obvykle `1883` |
| Username / Password | přihlašovací údaje brokeru, pokud je vyžaduje |
| Discovery prefix | `homeassistant`, pokud jste jej v nastavení HA neměnili |
| Zapnuto | zaškrtnutí — jinak se zařízení k brokeru nepřipojí |

Nastavení jde přímo do zařízení po místní síti — portál je neukládá.

![Okno Home Assistant v bloku „Integrace“ na portálu](../../img/storage-ha-portal-integration.png)
*Adresa brokeru, port a příznak „Zapnuto“ — vše, co zařízení potřebuje.*

## Krok 2. Najít zařízení v Home Assistant

`Settings` → `Devices & services` → karta **MQTT** → v sekci **Services** rozbalte uzel brokeru. Zařízení iDryer jsou vidět pod sériovými čísly ve tvaru `DEVICE_*`.

![Zařízení iDryer na stránce integrace MQTT](../../img/storage-ha-mqtt-devices.png)
*Zařízení pod uzlem brokeru; u Storage je vidět počet entit.*

Otevřete zařízení: HA už ukazuje hodnoty a ovládací prvky pásku.

## Krok 3. Sestavit kartu

HA si entity rozloží sám a vznikne dlouhý seznam. Hotové rozložení dá hodnoty nahoru a ovládání pásku do samostatného bloku.

1. `Settings` → `Dashboards` → **Add dashboard** → prázdný dashboard, otevřete jej.
2. Pravý horní roh → tužka (**Edit**) → nabídka „⋮“ → **Raw configuration editor**.
3. Vložte obsah níže a uložte.

Rozložení počítá s dashboardem typu `sections`.

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

![Raw configuration editor s vloženým rozložením](../../img/storage-ha-raw-editor.png)
*Totéž rozložení v editoru konfigurace dashboardu.*

Barva se zadává řetězcem v poli **Barva** — šestimístný HEX bez mřížky, například `FF8800`. Animace se vybírá ze seznamu, který zařízení publikuje samo: u starších firmwarů Storage je variant méně.

## Pokud se názvy entit neshodují

Rozložení počítá se standardními identifikátory ve tvaru `sensor.storage_temperature`. Pokud karta ukazuje „Entity not found“, podívejte se na své: `Settings` → `Devices & services` → **MQTT** → vaše zařízení → seznam entit — a nahraďte prefix v rozložení svým.

## Diagnostika

| Příznak | Co zkontrolovat |
|---|---|
| Zařízení se v HA neobjevilo | Na portálu má integrace Home Assistant zaškrtnuté „Zapnuto“, adresa a port brokeru jsou správné. Zařízení musí být `Online`. |
| Objevilo se, ale není teplota ani vlhkost | Snímač SHT31 není připojený nebo neodpovídá: bez snímače zařízení tyto entity nepublikuje. |
| Tlačítka nereagují | Zkontrolujte, že broker povoluje publikaci do témat `idryer/#` a že zařízení nemá v logu chyby autorizace. |
| Duchové entit s hodnotou `Unknown` | Zůstaly retained zprávy z předchozího firmwaru. Vyčistěte je: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
