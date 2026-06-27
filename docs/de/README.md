# Storage Link — Kurzbedienungsanleitung

Storage Link ist ein Modul basierend auf ESP32, das einen adressierbaren LED-Streifen in einen Statusanzeiger für ein Filamentspool-Lager verwandelt und optional Temperatur und Luftfeuchtigkeit von einem SHT31-Sensor veröffentlicht.

- Verbindet sich mit Wi-Fi und bindet das Gerät an [portal.idryer.org](https://portal.idryer.org/).
- Auf Befehl aus der Cloud oder der lokalen Anwendung beleuchtet es den Spulenplatz für eine festgelegte Zeit in einer festgelegten Farbe.
- Mit installiertem SHT31-Sensor veröffentlicht es Temperatur und Luftfeuchtigkeit.

Das Wissen über das Lager und die Spulen lebt in der externen Anwendung. Die Firmware funktioniert als einfaches Ausführungsmodul: „Platz N mit Farbe C für T Sekunden beleuchten". Dies ermöglicht, den Streifen an jedem beliebigen Regal anzubringen und diesen unabhängig von der Firmware im Portal zu beschreiben.

![Rack](../img/Rack.png)

## Unterstützte Boards

| Board                        |       |
| ---------------------------- | ----- |
| ESP32-C3 DevKitM-1           | `✅`  |
| ESP32-C3 Super Mini          | `✅`  |
| Seeed XIAO ESP32-S3          | `✅`  |
| Waveshare ESP32-S3-Zero      | `✅`  |

Jedes andere ESP32-C3- oder ESP32-S3-Board kann verwendet werden, wenn ein freier GPIO für die Streifendaten und ein Paar GPIO für I2C vorhanden sind. Überprüfen Sie das Pinout des Herstellers.

## Schaltplan

!!! warning "Verbinden Sie die Leitungen niemals an, während Strom anliegt, und trennen Sie sie nicht ab."

Storage Link steuert den Streifen über einen Signal-GPIO (`DATA`) und liest optional SHT31 über I2C.

![wiring diagram](../img/wiring-diagram.png)

### Stromversorgung des Streifens

Der Streifen und der ESP sollten von einer Stromquelle versorgt werden, die an die tatsächliche Belastung des Streifens angepasst ist.

- Bei vielen ESP-Boards wird der Pin `5V` (`VBUS`) direkt vom USB-Anschluss verdrahtet. Wenn das verwendete USB-Netzteil genug Strom mit Reserve für die Streifenbelastung liefert, können ESP und Streifen parallel von diesem Netzteil versorgt werden.
- Falls nicht genug Strom verfügbar ist, wird die Stromversorgung des Streifens auf ein separates 5-V-Netzteil ausgelagert. Der negative Anschluss des Netzteils muss mit dem `GND` des ESP verbunden werden — ohne gemeinsame Masse funktioniert das Signal `DATA` nicht.

In beiden Fällen muss im Menü `psu_ma` der Strom eingetragen werden, den Ihr Netzteil auf 5 V tatsächlich liefern kann. Dies ist nicht „wie viel gewünscht", sondern der Nennstromwert des Netzteils. FastLED begrenzt die Gesamthelligkeit basierend auf diesem Wert, um das Limit nicht zu überschreiten.

### Gute Montagepraktiken

Diese Elemente sind nicht erforderlich, um zu starten, beheben aber typische Probleme mit adressierbaren LED-Streifen (Pixel überspringen, „Glitches" der ersten LED, Spannungsabfälle beim Einschalten).

- **Widerstand in die `DATA`-Leitung.** Eine `300–500 Ω` Widerstand (typischerweise `390 Ω`) wird in Reihe zwischen dem GPIO des ESP und dem `DIN` des Streifens platziert, physikalisch so nah wie möglich am Streifen selbst. Dies reduziert Signalreflexionen und schützt den ersten Chip des Streifens.
- **Elektrolytkondensator zur Stromversorgung.** Zwischen `+5V` und `GND` am Stromeingangdes Streifens — `1000 µF` auf `16 V` (auch `25 V` ist gut, `10 V` ist das Minimum). Dies glättet Stromspitzen bei plötzlichen Aktivierungen.
- **Querschnittsfläche der gemeinsamen Masseleitung für den Netzstrom.** Die Masseleitung ESP—Streifen—Netzteil muss für den Spitzenstrom des Streifens dimensioniert sein. Richtwert für kurze Leitungen (bis ~1 m):

    | Netzstrom     | Querschnitt       | AWG       |
    | ------------- | ----------------- | --------- |
    | bis 3 A       | `0,5 mm²`         | `AWG 20`  |
    | bis 5 A       | `0,75 mm²`        | `AWG 18`  |


    Verwenden Sie für die `+5V`-Leitung zum Streifen die gleichen Querschnitte. Bei langen Streifen speisen Sie Strom von beiden Enden ein.

### Signalverbindungen

Die GPIO-Werte hängen vom Board ab.

#### ESP32-C3 DevKitM-1 und ESP32-C3 Super Mini

| ESP        | Funktion                    |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` des LED-Streifens    |
| `GPIO8`    | `SDA` (SHT31, optional)     |
| `GPIO9`    | `SCL` (SHT31, optional)     |
| `GND`      | gemeinsame Masse mit Streifen und Netzteil |

#### Seeed XIAO ESP32-S3

| ESP        | Funktion                    |
| ---------- | --------------------------- |
| `GPIO2`    | `DATA` des LED-Streifens    |
| `GPIO5`    | `SDA` (SHT31, optional)     |
| `GPIO6`    | `SCL` (SHT31, optional)     |
| `GND`      | gemeinsame Masse mit Streifen und Netzteil |

#### Waveshare ESP32-S3 Zero

| ESP        | Funktion                    |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` des LED-Streifens    |
| `GPIO8`    | `SDA` (SHT31, optional)     |
| `GPIO9`    | `SCL` (SHT31, optional)     |
| `GND`      | gemeinsame Masse mit Streifen und Netzteil |

### Board-Pinouts

ESP32-C3 Super Mini:

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

Waveshare ESP32-S3-Zero:

![Pinout Waveshare ESP32-S3-Zero](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

## Optionaler SHT31-Sensor

Der Sensor wird nur benötigt, wenn auf diesem Gerät Temperatur und Luftfeuchtigkeit veröffentlicht werden sollen. Storage Link startet und arbeitet mit dem Streifen identisch — mit oder ohne Sensor. Falls der Sensor nicht installiert ist, werden Temperatur und Luftfeuchtigkeit einfach nicht gesendet.

- Bus: I2C auf dem `SDA`/`SCL` des entsprechenden Boards.
- Adresse: `0x44` oder `0x45` (die Firmware bestimmt diese beim Start automatisch).

![SH31](../img/SHT31.png)

## Firmware-Flashing über Web-Flasher

Der Web-Flasher befindet sich auf [install.idryer.org](https://install.idryer.org/).

1. Verbinden Sie Storage Link über USB mit Ihrem Computer.
2. Öffnen Sie [install.idryer.org](https://install.idryer.org/) und klicken Sie auf die Schaltfläche **Storage Link**.
3. Wählen Sie Ihre Board-Variante.
4. Klicken Sie auf **Connect**, wählen Sie den seriellen Port. Falls das Gerät nicht erkannt wird, halten Sie die Taste `BOOT` auf dem Board gedrückt und drücken Sie kurz `RST`.
5. Klicken Sie auf **Install**. Der Flasher schreibt die Firmware.
6. Nach Abschluss des Flashens öffnet sich der Wi-Fi-Setup-Assistent.

## Wi-Fi-Konfiguration

Nach dem Flashen öffnet sich der Improv-Assistent automatisch im Serienport.

1. Geben Sie SSID und Passwort Ihres 2,4-GHz-Netzwerks ein.
2. Warten Sie auf den Status **Connected**.

Falls der Assistent nicht geöffnet wurde, trennen Sie die USB-Verbindung und verbinden Sie sie erneut über **Connect**, ohne die Firmware erneut zu flashen.

!!! note "ESP32-C3 und ESP32-S3 unterstützen nur Wi-Fi 2,4 GHz. 5-GHz-Netzwerke funktionieren nicht."

## Bindung an das Portal

1. Klicken Sie auf der Flasher-Seite auf **Claim ausführen**. Ein Claim-Befehl wird an das Gerät gesendet.
2. Nach einigen Sekunden wird auf der Seite eine PIN angezeigt. Die PIN ist etwa 5 Minuten gültig.
3. Öffnen Sie [portal.idryer.org](https://portal.idryer.org/) → **Gerät hinzufügen** → geben Sie die PIN ein.
4. Nach erfolgreicher Bindung wird das Gerät in der Online-Liste angezeigt.

Falls die PIN nicht angezeigt wurde oder die Bindung fehlgeschlagen ist, wiederholen Sie den Claim oder löschen Sie das Gerät im Portal und versuchen Sie es erneut.

## LED-Streifen-Konfiguration

Parameter werden über das Gerätekonfigurationsmenü festgelegt. Einige werden sofort angewendet, andere nur nach einem Neustart.

| Parameter         | Werte                                  | Standard | Anwendung   |
| ---------------- | ------------------------------------- | ------------ | -------------- |
| `led_count`      | `1..300`, Schrittweite `1`                     | `120`        | sofort         |
| `psu_ma`         | `500..20000` mA, Schrittweite `100`            | `5000`       | sofort         |
| Streifentyp      | Auswahl aus den verfügbaren im Menü             | `WS2812B`    | nach Neustart  |
| Farbfolge        | `GRB`, `RGB`, `BRG`, `BGR`            | `GRB`        | nach Neustart  |
| `language`       | `ru` / `en`                           | `en`         | sofort         |

Checkliste für die Grundkonfiguration nach dem ersten Start:

1. Legen Sie `led_count` fest — die tatsächliche Anzahl der Pixel auf dem Streifen.
2. Legen Sie `psu_ma` fest — der Nennstrom des 5-V-Netzteils in Milliampere.
3. Wählen Sie den Streifentyp, der bei Ihnen installiert ist.
4. Wählen Sie die Farbfolge. Standard ist `GRB`. Falls Rot und Grün vertauscht sind oder die Farbe falsch ist, probieren Sie andere Optionen.
5. Starten Sie das Gerät neu — Streifentyp und Farbfolge werden nur nach dem Neustart angewendet.

## Erwartetes Ergebnis

- Nach dem Claim ist das Gerät im Portal als online sichtbar.
- Der Beleuchtungsbefehl aus dem Portal oder der Anwendung beleuchtet den ausgewählten Platz auf dem Streifen für die angegebene Zeit. Ein neuer Befehl schaltet den vorherigen Platz aus und beleuchtet den nächsten.
- Falls SHT31 installiert ist — Temperatur und Luftfeuchtigkeit werden regelmäßig im Portal aktualisiert.
- Falls SHT31 nicht installiert ist — Klimamesswerte fehlen, das ist normal.
