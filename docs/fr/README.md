# Storage Link — Guide rapide

Storage Link est un module basé sur ESP32 qui transforme un ruban LED adressable en indicateur de rangement pour les bobines de filament et publie optionnellement la température et l'humidité depuis un capteur SHT31.

- Se connecte au Wi-Fi et associe l'appareil à [portal.idryer.org](https://portal.idryer.org/).
- Sur commande du cloud ou d'une application locale, éclaire l'emplacement de la bobine dans une couleur spécifiée pour une durée définie.
- Avec un capteur SHT31 installé, publie la température et l'humidité.

La connaissance du rangement et des bobines réside dans l'application externe. Le micrologiciel fonctionne comme un simple module exécutif : « éclairer l'emplacement N avec la couleur C pendant T secondes ». Cela permet de coller le ruban sur n'importe quelle étagère et la décrire dans le portail indépendamment du micrologiciel.

![Rack](../img/Rack.png)

## Cartes supportées

| Carte                        |       |
| ---------------------------- | ----- |
| ESP32-C3 DevKitM-1           | `✅`  |
| ESP32-C3 Super Mini          | `✅`  |
| Seeed XIAO ESP32-S3          | `✅`  |
| Waveshare ESP32-S3-Zero      | `✅`  |

N'importe quelle autre carte basée sur ESP32-C3 ou ESP32-S3 peut être utilisée s'il existe une GPIO libre pour les données du ruban et une paire de GPIO pour I2C. Consultez le diagramme de broches du fabricant.

## Schéma de connexion

!!! warning "Ne branchez et ne débranchez jamais les câbles sous tension."

Storage Link commande le ruban via une GPIO de signal unique (`DATA`) et lit optionnellement le SHT31 via I2C.

![wiring diagram](../img/wiring-diagram.png)

### Alimentation du ruban

Le ruban et l'ESP doivent être alimentés par une source dimensionnée en courant pour la charge réelle du ruban.

- Sur de nombreuses cartes ESP, la broche `5V` (`VBUS`) est directement issue du connecteur USB. Si le bloc d'alimentation USB utilisé fournit suffisamment de courant pour la charge du ruban, l'ESP et le ruban peuvent être alimentés en parallèle à partir de celui-ci.
- S'il n'y a pas assez de marge de courant, l'alimentation du ruban est fournie par une alimentation 5 V séparée. Le négatif de l'alimentation doit impérativement être relié à `GND` de l'ESP — sans masse commune, le signal `DATA` ne fonctionnera pas.

Dans les deux cas, le menu `psu_ma` doit spécifier le courant que votre bloc d'alimentation peut réellement fournir en 5 V. C'est la valeur nominale de l'alimentation, pas une valeur souhaitée. FastLED limitera la luminosité globale en fonction de cette valeur pour ne pas dépasser la limite.

### Bonnes pratiques de montage

Ces éléments ne sont pas obligatoires pour le démarrage, mais éliminent les problèmes typiques avec les rubans adressables (pixels manquants, « défauts » du premier LED, baisse de tension au démarrage).

- **Résistance en série sur `DATA`.** Placez une résistance `300–500 Ω` (généralement `390 Ω`) en série entre la GPIO de l'ESP et `DIN` du ruban, physiquement aussi proche que possible du ruban. Elle atténue les réflexions de signal et protège la première puce du ruban.
- **Condensateur électrolytique sur l'alimentation.** Entre `+5V` et `GND` à l'entrée d'alimentation du ruban — `1000 µF` à `16 V` (ou `25 V` c'est aussi bien, `10 V` c'est le minimum). Lisse les pics de courant lors des mises en marche brutales.
- **Section du conducteur de masse par courant de l'alimentation.** Le fil de masse ESP—ruban—alimentation doit être dimensionné pour le courant de pointe du ruban. Référence pour les fils courts (jusqu'à ~1 m) :

    | Courant alim | Section           | AWG       |
    | ------------ | ----------------- | --------- |
    | jusqu'à 3 A  | `0,5 mm²`         | `AWG 20`  |
    | jusqu'à 5 A  | `0,75 mm²`        | `AWG 18`  |

    Pour la ligne `+5V` vers le ruban, utilisez les mêmes sections. Sur les longs rubans, alimentez depuis les deux extrémités.

### Connexions des signaux

Les valeurs GPIO dépendent de la carte.

#### ESP32-C3 DevKitM-1 et ESP32-C3 Super Mini

| ESP        | Destination                 |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` du ruban adressable  |
| `GPIO8`    | `SDA` (SHT31, optionnel)    |
| `GPIO9`    | `SCL` (SHT31, optionnel)    |
| `GND`      | masse commune avec ruban et alimentation |

#### Seeed XIAO ESP32-S3

| ESP        | Destination                 |
| ---------- | --------------------------- |
| `GPIO2`    | `DATA` du ruban adressable  |
| `GPIO5`    | `SDA` (SHT31, optionnel)    |
| `GPIO6`    | `SCL` (SHT31, optionnel)    |
| `GND`      | masse commune avec ruban et alimentation |

#### Waveshare ESP32-S3 Zero

| ESP        | Destination                 |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` du ruban adressable  |
| `GPIO8`    | `SDA` (SHT31, optionnel)    |
| `GPIO9`    | `SCL` (SHT31, optionnel)    |
| `GND`      | masse commune avec ruban et alimentation |

### Diagramme de broches des cartes

ESP32-C3 Super Mini :

![Diagramme de broches ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

Waveshare ESP32-S3-Zero :

![Diagramme de broches Waveshare ESP32-S3-Zero](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

## Capteur SHT31 optionnel

Le capteur n'est nécessaire que si vous souhaitez publier la température et l'humidité sur cet appareil. Storage Link démarre et fonctionne avec le ruban de la même manière, avec ou sans capteur. Si le capteur n'est pas installé, la température et l'humidité ne sont simplement pas envoyées.

- Bus : I2C sur `SDA`/`SCL` de la carte correspondante.
- Adresse : `0x44` ou `0x45` (le micrologiciel déterminera automatiquement au démarrage).

![SH31](../img/SHT31.png)

## Programmation via le flasheur web

Le flasheur web se trouve sur [install.idryer.org](https://install.idryer.org/).

1. Connectez Storage Link au port USB de votre ordinateur.
2. Ouvrez [install.idryer.org](https://install.idryer.org/) et cliquez sur le bouton **Storage Link**.
3. Sélectionnez votre variante de carte.
4. Cliquez sur **Connect**, sélectionnez le port série. Si l'appareil n'est pas reconnu, maintenez le bouton `BOOT` sur la carte et appuyez brièvement sur `RST`.
5. Cliquez sur **Install**. Le flasheur écrira le micrologiciel.
6. Une fois la programmation terminée, l'assistant de configuration Wi-Fi s'ouvrira.

## Configuration Wi-Fi

Après la programmation, l'assistant Improv s'ouvre automatiquement sur le port série.

1. Entrez le SSID et le mot de passe de votre réseau 2,4 GHz.
2. Attendez le statut **Connected**.

Si l'assistant ne s'est pas ouvert, débranchez l'USB et reconnectez via **Connect** sans reprogrammer.

!!! note "ESP32-C3 et ESP32-S3 ne supportent que le Wi-Fi 2,4 GHz. Les réseaux 5 GHz ne fonctionnent pas."

## Association au portail

1. Sur la page du flasheur, cliquez sur **Connecter et effectuer Claim**. Une commande claim sera envoyée à l'appareil.
2. Au bout de quelques secondes, un PIN apparaîtra sur la page. Le PIN est valide pendant environ 5 minutes.
3. Ouvrez [portal.idryer.org](https://portal.idryer.org/) → **Ajouter un appareil** → entrez le PIN.
4. Une fois l'association réussie, l'appareil apparaîtra dans la liste en ligne.

Si le PIN n'a pas apparu ou si l'association a échoué — répétez le claim, ou supprimez l'appareil dans le portail et réessayez.

## Configuration du ruban

Les paramètres sont définis via le menu de configuration de l'appareil. Certains s'appliquent immédiatement, d'autres seulement après un redémarrage.

| Paramètre        | Valeurs                               | Défaut     | Application |
| ---------------- | ------------------------------------- | ---------- | ----------- |
| `led_count`      | `1..300`, pas `1`                     | `120`      | immédiat    |
| `psu_ma`         | `500..20000` mA, pas `100`            | `5000`     | immédiat    |
| type de ruban    | choix dans le menu disponible         | `WS2812B`  | après reboot |
| ordre des couleurs | `GRB`, `RGB`, `BRG`, `BGR`            | `GRB`      | après reboot |
| `language`       | `ru` / `en`                           | `en`       | immédiat    |

Liste de contrôle basique après le premier lancement :

1. Définissez `led_count` — le nombre réel de pixels sur le ruban.
2. Définissez `psu_ma` — le courant nominal de l'alimentation 5 V en milliampères.
3. Sélectionnez le type de ruban que vous avez installé.
4. Sélectionnez l'ordre des couleurs. Par défaut `GRB`. Si le rouge et le vert sont échangés ou la couleur est incorrecte — essayez les différentes variantes.
5. Redémarrez l'appareil — le type de ruban et l'ordre des couleurs ne s'appliquent qu'après reboot.

## Ce qui devrait se passer

- Après claim, l'appareil est visible dans le portail en ligne.
- Une commande d'éclairage depuis le portail ou l'application allume l'emplacement sélectionné sur le ruban pendant le temps spécifié. Une nouvelle commande éteint l'emplacement précédent et en allume un nouveau.
- Si SHT31 est installé — la température et l'humidité se mettent à jour régulièrement dans le portail.
- Si SHT31 n'est pas installé — les données climatiques sont absentes, c'est normal.
