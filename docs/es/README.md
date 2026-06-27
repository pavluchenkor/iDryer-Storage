# Storage Link — guía rápida

Storage Link es un módulo basado en ESP32 que convierte una tira LED direccionable en un indicador de almacenamiento de bobinas de filamento y, opcionalmente, publica temperatura y humedad desde un sensor SHT31.

- Se conecta a Wi-Fi y vincula el dispositivo a [portal.idryer.org](https://portal.idryer.org/).
- Por comando desde la nube o desde la aplicación local, ilumina la ranura de la bobina durante un tiempo especificado con el color especificado.
- Con el sensor SHT31 instalado, publica temperatura y humedad.

La información sobre el almacenamiento y las bobinas reside en la aplicación externa. El firmware actúa como un módulo ejecutor simple: «ilumina la ranura N con el color C durante T segundos». Esto permite pegar la tira en cualquier estantería y describirla en el portal independientemente del firmware.

![Rack](../img/Rack.png)

## Placas soportadas

| Placa                        |       |
| ---------------------------- | ----- |
| ESP32-C3 DevKitM-1           | `✅`  |
| ESP32-C3 Super Mini          | `✅`  |
| Seeed XIAO ESP32-S3          | `✅`  |
| Waveshare ESP32-S3-Zero      | `✅`  |

Se puede utilizar cualquier otra placa basada en ESP32-C3 o ESP32-S3 si tiene un GPIO libre para los datos de la tira y un par de GPIO para I2C. Consulte el pinout del fabricante.

## Esquema de conexión

!!! warning "Nunca conecte o desconecte cables con la alimentación aplicada."

Storage Link controla la tira a través de un GPIO de señal (`DATA`) y opcionalmente lee SHT31 por I2C.

![wiring diagram](../img/wiring-diagram.png)

### Alimentación de la tira

La tira y el ESP deben alimentarse desde una fuente acordada en corriente con la carga real de la tira.

- En muchas placas ESP, el pin `5V` (`VBUS`) se proporciona directamente desde el conector USB. Si el bloque de alimentación USB utilizado entrega corriente con margen para la carga de la tira, el ESP y la tira pueden alimentarse en paralelo desde él.
- Si no hay margen de corriente, la alimentación de la tira se conecta a una PSU 5V separada. El negativo de la PSU debe conectarse al `GND` del ESP: sin tierra común, la señal `DATA` no funcionará.

En ambos casos, en el menú `psu_ma` debe especificar la corriente que su bloque de alimentación realmente puede entregar a 5V. Esto no es «cuánto quiere», sino la especificación de corriente de la PSU. FastLED limitará el brillo combinado según este valor para no exceder el límite.

### Buenas prácticas de montaje

Estos elementos no son obligatorios para que funcione, pero eliminan problemas típicos con tiras LED direccionables (píxeles perdidos, «fallos» del primer LED, caídas de voltaje al encender).

- **Resistencia en la línea `DATA`.** Coloque una resistencia de `300–500 Ω` (típicamente `390 Ω`) en serie entre el GPIO del ESP y la entrada `DIN` de la tira, físicamente lo más cerca posible de la tira misma. Atenúa las reflexiones de señal y protege el primer chip de la tira.
- **Condensador electrolítico en la alimentación.** Entre `+5V` y `GND` en la entrada de alimentación de la tira: `1000 µF` a `16 V` (a `25 V` también es bueno, a `10 V` es el mínimo). Suaviza los picos de corriente en encendidos bruscos.
- **Sección del conductor común de tierra según la corriente de la PSU.** El cable de tierra ESP—tira—PSU debe estar dimensionado para la corriente pico de la tira. Referencia para cables cortos (hasta ~1 m):

    | Corriente de PSU  | Sección           | AWG       |
    | --------------- | ----------------- | --------- |
    | hasta 3 A       | `0,5 mm²`         | `AWG 20`  |
    | hasta 5 A       | `0,75 mm²`        | `AWG 18`  |


    Para la línea `+5V` a la tira, use las mismas secciones. En tiras largas, suministre alimentación desde ambos extremos.

### Conexiones de señales

Los valores de GPIO dependen de la placa.

#### ESP32-C3 DevKitM-1 y ESP32-C3 Super Mini

| ESP        | Propósito                   |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` de tira direccionable|
| `GPIO8`    | `SDA` (SHT31, opcional)     |
| `GPIO9`    | `SCL` (SHT31, opcional)     |
| `GND`      | tierra común con tira y PSU |

#### Seeed XIAO ESP32-S3

| ESP        | Propósito                   |
| ---------- | --------------------------- |
| `GPIO2`    | `DATA` de tira direccionable|
| `GPIO5`    | `SDA` (SHT31, opcional)     |
| `GPIO6`    | `SCL` (SHT31, opcional)     |
| `GND`      | tierra común con tira y PSU |

#### Waveshare ESP32-S3 Zero

| ESP        | Propósito                   |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` de tira direccionable|
| `GPIO8`    | `SDA` (SHT31, opcional)     |
| `GPIO9`    | `SCL` (SHT31, opcional)     |
| `GND`      | tierra común con tira y PSU |

### Pinout de las placas

ESP32-C3 Super Mini:

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

Waveshare ESP32-S3-Zero:

![Pinout Waveshare ESP32-S3-Zero](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

## Sensor SHT31 opcional

El sensor se necesita solo si desea publicar temperatura y humedad en este dispositivo. Storage Link se inicia y funciona con la tira de la misma manera, con o sin sensor. Si el sensor no está instalado, la temperatura y la humedad simplemente no se envían.

- Bus: I2C en `SDA`/`SCL` de la placa correspondiente.
- Dirección: `0x44` o `0x45` (el firmware lo detectará automáticamente al iniciar).

![SH31](../img/SHT31.png)

## Flasheo a través del flasher web

El flasher web se encuentra en [install.idryer.org](https://install.idryer.org/).

1. Conecte Storage Link al puerto USB de su computadora.
2. Abra [install.idryer.org](https://install.idryer.org/) y haga clic en el botón **Storage Link**.
3. Seleccione su variante de placa.
4. Haga clic en **Connect** y seleccione el puerto serie. Si el dispositivo no se detecta, mantenga presionado el botón `BOOT` en la placa y presione brevemente `RST`.
5. Haga clic en **Install**. El flasher escribirá el firmware.
6. Una vez completado el flasheo, se abrirá el asistente de configuración de Wi-Fi.

## Configuración de Wi-Fi

Después del flasheo, el asistente Improv se abre automáticamente en el puerto serie.

1. Ingrese el SSID y contraseña de su red 2,4 GHz.
2. Espere a que aparezca el estado **Connected**.

Si el asistente no se abre, desconecte USB y reconecte a través de **Connect** sin volver a flashear.

!!! note "ESP32-C3 y ESP32-S3 solo soportan Wi-Fi 2,4 GHz. Las redes 5 GHz no funcionan."

## Vinculación al portal

1. En la página del flasher, haga clic en **Conectar y ejecutar Claim**. El dispositivo recibirá un comando claim.
2. Después de unos segundos, aparecerá un PIN en la página. El PIN es válido durante aproximadamente 5 minutos.
3. Abra [portal.idryer.org](https://portal.idryer.org/) → **Agregar dispositivo** → ingrese el PIN.
4. Después de una vinculación exitosa, el dispositivo aparecerá en la lista en línea.

Si el PIN no aparece o la vinculación no se completa, repita el claim o elimine el dispositivo en el portal e intente de nuevo.

## Configuración de la tira

Los parámetros se establecen a través del menú de configuración del dispositivo. Algunos se aplican inmediatamente, otros solo después de reiniciar.

| Parámetro        | Valores                               | Predeterminado | Aplicación     |
| ---------------- | ------------------------------------- | -------------- | -------------- |
| `led_count`      | `1..300`, paso `1`                    | `120`          | inmediato      |
| `psu_ma`         | `500..20000` mA, paso `100`           | `5000`         | inmediato      |
| tipo de tira     | seleccione de disponibles en menú    | `WS2812B`      | después reboot |
| orden de colores | `GRB`, `RGB`, `BRG`, `BGR`            | `GRB`          | después reboot |
| `language`       | `ru` / `en`                           | `en`           | inmediato      |

Lista de verificación básica después del primer inicio:

1. Establezca `led_count` al número real de píxeles en la tira.
2. Establezca `psu_ma` a la corriente especificada de la PSU 5V en miliamperios.
3. Seleccione el tipo de tira que tiene instalado.
4. Seleccione el orden de colores. El predeterminado es `GRB`. Si el rojo y el verde están intercambiados o el color es incorrecto, pruebe otras opciones.
5. Reinicie el dispositivo: el tipo de tira y el orden de colores solo se aplican después de reiniciar.

## Qué debería obtener

- Después del claim, el dispositivo es visible en el portal en línea.
- El comando de iluminación desde el portal o la aplicación enciende la ranura seleccionada en la tira durante el tiempo especificado. Un nuevo comando apaga la ranura anterior y enciende la siguiente.
- Si está instalado SHT31, la temperatura y humedad se actualizan periódicamente en el portal.
- Si SHT31 no está instalado, las lecturas climáticas están ausentes, esto es normal.
