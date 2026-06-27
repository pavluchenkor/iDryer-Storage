# Storage Link — stručný průvodce

Storage Link je modul na ESP32, který přeměňuje adresný LED pás na indikátor skladu cívek filamentu a volitelně zveřejňuje teplotu a vlhkost ze snímače SHT31.

- Připojuje se k Wi-Fi a vážete zařízení s [portal.idryer.org](https://portal.idryer.org/).
- Na příkaz z cloudu nebo místní aplikace osvětlí vybranou pozici cívky na určitou dobu zvolenou barvou.
- Pokud je nainstalován snímač SHT31, zveřejňuje teplotu a vlhkost.

Znalosti o skladu a cívkách jsou umístěny v externí aplikaci. Firmware funguje jako jednoduchý výkonný modul: "osvětlete pozici N barvou C po dobu T sekund". To umožňuje přilepit pás na libovolný regál a popsat jej na portálu nezávisle na firmware.

![Rack](../img/Rack.png)

## Podporované desky

| Deska                        |       |
| ---------------------------- | ----- |
| ESP32-C3 DevKitM-1           | `✅`  |
| ESP32-C3 Super Mini          | `✅`  |
| Seeed XIAO ESP32-S3          | `✅`  |
| Waveshare ESP32-S3-Zero      | `✅`  |

Můžete použít jakoukoli jinou desku na ESP32-C3 nebo ESP32-S3, pokud máte volný GPIO pro data pásu a pár GPIO pro I2C. Zkontrolujte si pinout výrobce.

## Schéma zapojení

!!! warning "Nikdy nepřipojujte ani neodpojujte dráty při zapojeném napájení."

Storage Link řídí pás jedním signálním GPIO (`DATA`) a volitelně čte SHT31 přes I2C.

![wiring diagram](../img/wiring-diagram.png)

### Napájení pásu

Pás i ESP by měly být napájeny ze zdroje, který je v souladu s proudem skutečné zátěže pásu.

- Na mnoha deskách ESP je pin `5V` (`VBUS`) vyveden přímo z USB konektoru. Pokud má používaný USB napájecí blok proudu s rezervou pro zátěž pásu, je povoleno napájet ESP a pás paralelně z něj.
- Pokud není k dispozici dostatečný proud — napájení pásu je odděleno na samostatný zdroj 5 V. Mínus zdroje musí být připojen k `GND` ESP — bez společné zeminy signál `DATA` nebude fungovat.

V obou případech je v nabídce `psu_ma` nutné zadat proud, který je váš napájecí zdroj skutečně schopen dodat na 5 V. To není "kolik chcete", ale jmenovitý výstup zdroje. FastLED podle tohoto hodnotu omezí kombinovanou jas, aby se nepřekročil limit.

### Dobré praktiky montáže

Tyto prvky nejsou povinné pro spuštění, ale odstraňují typické problémy s adresným pásem (chybějící pixely, "šoky" prvního LED, pokles při zapnutí).

- **Rezistor v lince `DATA`.** V sérii mezi GPIO ESP a `DIN` pásu je umístěn rezistor `300–500 Ω` (typicky `390 Ω`), fyzicky co nejblíže k samotnému pásu. Tlumí odrazy signálu a chrání první čip pásu.
- **Elektrolyt na napájení.** Mezi `+5V` a `GND` na vstupu napájení pásu — `1000 µF` na `16 V` (na `25 V` — také dobré, na `10 V` — minimum). Vyhladí proudové nárazy při náhlých zapnutích.
- **Průřez společné zeminy podle proudu zdroje.** Zemní vodič ESP—pás—zdroj by měl být dimenzován pro špičkový proud pásu. Orientace pro krátké vodiče (do ~1 m):

    | Proud zdroje  | Průřez            | AWG       |
    | ------------- | ----------------- | --------- |
    | do 3 A        | `0,5 mm²`         | `AWG 20`  |
    | do 5 A        | `0,75 mm²`        | `AWG 18`  |


    Pro linku `+5V` k pásu — stejné průřezy. Na dlouhých pásech napájejte z obou konců.

### Připojení signálů

Hodnoty GPIO závisí na desce.

#### ESP32-C3 DevKitM-1 a ESP32-C3 Super Mini

| ESP        | Účel                        |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` adresného pásu       |
| `GPIO8`    | `SDA` (SHT31, volitelně)    |
| `GPIO9`    | `SCL` (SHT31, volitelně)    |
| `GND`      | společná zem s pásem a zdrojem |

#### Seeed XIAO ESP32-S3

| ESP        | Účel                        |
| ---------- | --------------------------- |
| `GPIO2`    | `DATA` adresného pásu       |
| `GPIO5`    | `SDA` (SHT31, volitelně)    |
| `GPIO6`    | `SCL` (SHT31, volitelně)    |
| `GND`      | společná zem s pásem a zdrojem |

#### Waveshare ESP32-S3 Zero

| ESP        | Účel                        |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` adresného pásu       |
| `GPIO8`    | `SDA` (SHT31, volitelně)    |
| `GPIO9`    | `SCL` (SHT31, volitelně)    |
| `GND`      | společná zem s pásem a zdrojem |

### Pinout desek

ESP32-C3 Super Mini:

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

Waveshare ESP32-S3-Zero:

![Pinout Waveshare ESP32-S3-Zero](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

## Volitelný snímač SHT31

Snímač je potřebný pouze pokud na tomto zařízení chcete zveřejňovat teplotu a vlhkost. Storage Link se spouští a pracuje s pásem stejně — se snímačem nebo bez. Pokud snímač není nainstalován, teplota a vlhkost se jednoduše nezveřejňují.

- Sběrnice: I2C na `SDA`/`SCL` příslušné desky.
- Adresa: `0x44` nebo `0x45` (firmware si ji sám určí při spuštění).

![SH31](../img/SHT31.png)

## Firmware prostřednictvím webového flasheru

Webový flasher je umístěn na [install.idryer.org](https://install.idryer.org/).

1. Připojte Storage Link k USB portu počítače.
2. Otevřete [install.idryer.org](https://install.idryer.org/) a klikněte na tlačítko **Storage Link**.
3. Vyberte variantu vaší desky.
4. Klikněte na **Connect**, vyberte sériový port. Pokud zařízení není rozpoznáno, stiskněte tlačítko `BOOT` na desce a krátce stiskněte `RST`.
5. Klikněte na **Install**. Flasher zapíše firmware.
6. Po dokončení flashování se otevře průvodce nastavením Wi-Fi.

## Nastavení Wi-Fi

Po flashování se Improv průvodce automaticky otevře v sériovém portu.

1. Zadejte SSID a heslo vaší sítě 2,4 GHz.
2. Počkejte na stav **Connected**.

Pokud se průvodce neotevřel, odpojte USB a připojte jej znovu pomocí **Connect** bez opětovného flashování.

!!! note "ESP32-C3 a ESP32-S3 podporují pouze Wi-Fi 2,4 GHz. Sítě 5 GHz nefungují."

## Vázání na portál

1. Na stránce flasheru klikněte na **Připojit a provést Claim**. Na zařízení bude odeslán příkaz claim.
2. Za několik sekund se na stránce objeví PIN. PIN je platný přibližně 5 minut.
3. Otevřete [portal.idryer.org](https://portal.idryer.org/) → **Přidat zařízení** → zadejte PIN.
4. Po úspěšném vázání se zařízení objeví v seznamu online.

Pokud se PIN neobjevil nebo se vázání nepodařilo — opakujte claim, nebo odstraňte zařízení na portálu a zkuste znovu.

## Nastavení pásu

Parametry se nastavují prostřednictvím nabídky konfigurace zařízení. Některé se aplikují okamžitě, některé pouze po restartování.

| Parametr         | Hodnoty                               | Výchozí    | Aplikace   |
| ---------------- | ------------------------------------- | ---------- | ---------- |
| `led_count`      | `1..300`, krok `1`                    | `120`      | okamžitě   |
| `psu_ma`         | `500..20000` mA, krok `100`           | `5000`     | okamžitě   |
| typ pásu         | výběr z dostupných v nabídce          | `WS2812B`  | po reboot  |
| pořadí barev     | `GRB`, `RGB`, `BRG`, `BGR`            | `GRB`      | po reboot  |
| `language`       | `ru` / `en`                           | `en`       | okamžitě   |

Základní kontrolní seznam po prvním spuštění:

1. Nastavte `led_count` — skutečný počet pixelů na pásu.
2. Nastavte `psu_ma` — jmenovitý proud zdroje na 5 V v miliampérech.
3. Vyberte typ pásu, který máte nainstalován.
4. Vyberte pořadí barev. Výchozí je `GRB`. Pokud jsou červená a zelená zaměňovány nebo je barva nesprávná — vyzkoušejte varianty.
5. Restartujte zařízení — typ pásu a pořadí barev se aplikují pouze po reboot.

## Co by mělo být výsledkem

- Po claim je zařízení viditelné na portálu online.
- Příkaz osvětlení z portálu nebo aplikace zapálí vybranou pozici na pásu na stanovenou dobu. Nový příkaz zhasne předchozí pozici a zapálí další.
- Pokud je nainstalován SHT31 — teplota a vlhkost se na portálu pravidelně aktualizují.
- Pokud SHT31 není nainstalován — údaje o klimatu chybějí, to je normální.
