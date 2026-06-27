# Storage Link — guia rápido

Storage Link é um módulo baseado em ESP32 que transforma uma fita LED endereçável num indicador de armazenamento de carretilhas de filamento, e opcionalmente publica temperatura e humidade de um sensor SHT31.

- Conecta-se a Wi-Fi e associa o dispositivo a [portal.idryer.org](https://portal.idryer.org/).
- Por comando da nuvem ou aplicação local, ilumina o slot da carretilha durante um tempo especificado com uma cor especificada.
- Com o sensor SHT31 instalado, publica temperatura e humidade.

O conhecimento sobre o armazenamento e carretilhas reside na aplicação externa. O firmware funciona como um módulo executivo simples: «ilumina o slot N com a cor C durante T segundos». Isto permite colar a fita em qualquer estante e descrevê-la no portal independentemente do firmware.

![Rack](../img/Rack.png)

## Placas suportadas

| Placa                        |       |
| ---------------------------- | ----- |
| ESP32-C3 DevKitM-1           | `✅`  |
| ESP32-C3 Super Mini          | `✅`  |
| Seeed XIAO ESP32-S3          | `✅`  |
| Waveshare ESP32-S3-Zero      | `✅`  |

Qualquer outra placa baseada em ESP32-C3 ou ESP32-S3 pode ser utilizada, desde que haja um GPIO livre para os dados da fita e um par de GPIO para I2C. Consulte o pinout do fabricante.

## Diagrama de ligação

!!! warning "Nunca ligue ou desligue os fios com alimentação ligada."

Storage Link controla a fita por um GPIO de sinal único (`DATA`) e opcionalmente lê o SHT31 via I2C.

![wiring diagram](../img/wiring-diagram.png)

### Alimentação da fita

A fita e o ESP devem ser alimentados por uma fonte adequada em termos de corrente à carga real da fita.

- Em muitas placas ESP, o pino `5V` (`VBUS`) está ligado directamente ao conector USB. Se o adaptador USB utilizado fornecer corrente suficiente para alimentar a fita, é permitido alimentar tanto o ESP como a fita em paralelo a partir dele.
- Se não houver corrente suficiente — a alimentação da fita é fornecida por uma fonte de 5 V separada. O negativo da fonte deve estar ligado a `GND` do ESP — sem uma terra comum, o sinal `DATA` não funcionará.

Em ambos os casos, no menu `psu_ma` deve ser introduzida a corrente que a sua fonte de alimentação é realmente capaz de fornecer a 5 V. Isto não é «quanto deseja», mas sim a saída nominal da fonte. FastLED utilizará este valor para limitar o brilho total de modo a não exceder o limite.

### Boas práticas de montagem

Estes elementos não são obrigatórios para o funcionamento, mas eliminam problemas típicos com fitas endereçáveis (píxeis perdidos, «erros» do primeiro LED, quedas durante a activação).

- **Resistência na linha `DATA`.** Coloque um resistor `300–500 Ω` (tipicamente `390 Ω`) em série entre o GPIO do ESP e o `DIN` da fita, o mais próximo possível da fita. Isto elimina reflexos do sinal e protege o primeiro chip da fita.
- **Condensador electrolítico na alimentação.** Entre `+5V` e `GND` na entrada de alimentação da fita — `1000 µF` a `16 V` (a `25 V` também é adequado, a `10 V` é o mínimo). Suaviza picos de corrente durante activações abruptas.
- **Secção transversal da terra comum de acordo com a corrente da fonte.** O fio terra ESP—fita—fonte deve ser dimensionado para a corrente de pico da fita. Orientação para fios curtos (até ~1 m):

    | Corrente da fonte | Secção transversal | AWG       |
    | ------------- | ----------------- | --------- |
    | até 3 A       | `0,5 mm²`         | `AWG 20`  |
    | até 5 A       | `0,75 mm²`        | `AWG 18`  |


    Para a linha `+5V` para a fita — as mesmas secções. Em fitas longas, alimente a partir de ambas as extremidades.

### Ligações de sinais

Os valores GPIO dependem da placa.

#### ESP32-C3 DevKitM-1 e ESP32-C3 Super Mini

| ESP        | Designação                  |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` da fita endereçável  |
| `GPIO8`    | `SDA` (SHT31, opcional)     |
| `GPIO9`    | `SCL` (SHT31, opcional)     |
| `GND`      | terra comum com fita e fonte|

#### Seeed XIAO ESP32-S3

| ESP        | Designação                  |
| ---------- | --------------------------- |
| `GPIO2`    | `DATA` da fita endereçável  |
| `GPIO5`    | `SDA` (SHT31, opcional)     |
| `GPIO6`    | `SCL` (SHT31, opcional)     |
| `GND`      | terra comum com fita e fonte|

#### Waveshare ESP32-S3 Zero

| ESP        | Designação                  |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` da fita endereçável  |
| `GPIO8`    | `SDA` (SHT31, opcional)     |
| `GPIO9`    | `SCL` (SHT31, opcional)     |
| `GND`      | terra comum com fita e fonte|

### Pinout das placas

ESP32-C3 Super Mini:

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

Waveshare ESP32-S3-Zero:

![Pinout Waveshare ESP32-S3-Zero](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

## Sensor SHT31 opcional

O sensor é necessário apenas se pretender publicar temperatura e humidade neste dispositivo. Storage Link inicia e funciona com a fita da mesma forma — com ou sem o sensor. Se o sensor não estiver instalado, a temperatura e humidade simplesmente não são enviadas.

- Barramento: I2C no `SDA`/`SCL` da placa correspondente.
- Endereço: `0x44` ou `0x45` (o firmware detectará automaticamente no arranque).

![SH31](../img/SHT31.png)

## Flash via web-flasher

O web-flasher está disponível em [install.idryer.org](https://install.idryer.org/).

1. Ligue o Storage Link à porta USB do computador.
2. Abra [install.idryer.org](https://install.idryer.org/) e clique no botão **Storage Link**.
3. Seleccione a sua variante de placa.
4. Clique em **Connect**, seleccione a porta serial. Se o dispositivo não for detectado, mantenha pressionado o botão `BOOT` na placa e clique brevemente em `RST`.
5. Clique em **Install**. O flasher escreverá o firmware.
6. Após a conclusão do flash, o assistente de configuração Wi-Fi será aberto.

## Configuração Wi-Fi

Após o flash, o assistente Improv abre-se automaticamente na porta serial.

1. Introduza o SSID e a palavra-passe da sua rede 2,4 GHz.
2. Aguarde o estado **Connected**.

Se o assistente não abrir, desconecte o USB e reconecte via **Connect** sem fazer flash novamente.

!!! note "ESP32-C3 e ESP32-S3 suportam apenas Wi-Fi 2,4 GHz. Redes 5 GHz não funcionam."

## Associação ao portal

1. Na página do flasher, clique em **Ligar e executar Claim**. Um comando claim será enviado para o dispositivo.
2. Após alguns segundos, um PIN aparecerá na página. O PIN é válido durante cerca de 5 minutos.
3. Abra [portal.idryer.org](https://portal.idryer.org/) → **Adicionar dispositivo** → introduza o PIN.
4. Após a associação bem-sucedida, o dispositivo aparecerá na lista online.

Se o PIN não aparecer ou a associação não funcionar — repita o claim, ou elimine o dispositivo no portal e tente novamente.

## Configuração da fita

Os parâmetros são definidos através do menu de configuração do dispositivo. Alguns são aplicados imediatamente, outros apenas após reinicialização.

| Parâmetro        | Valores                               | Padrão   | Aplicação      |
| ---------------- | ------------------------------------- | -------- | -------------- |
| `led_count`      | `1..300`, passo `1`                   | `120`    | imediatamente  |
| `psu_ma`         | `500..20000` mA, passo `100`          | `5000`   | imediatamente  |
| tipo de fita     | seleccione entre as disponíveis       | `WS2812B`| após reboot    |
| ordem de cores   | `GRB`, `RGB`, `BRG`, `BGR`            | `GRB`    | após reboot    |
| `language`       | `ru` / `en`                           | `en`     | imediatamente  |

Lista de verificação básica após o primeiro arranque:

1. Defina `led_count` — o número real de píxeis na fita.
2. Defina `psu_ma` — a corrente nominal da fonte para 5 V em miliamperes.
3. Seleccione o tipo de fita que tem instalado.
4. Seleccione a ordem de cores. Por predefinção `GRB`. Se o vermelho e o verde estão trocados ou a cor está incorrecta — experimente as outras opções.
5. Reinicie o dispositivo — o tipo de fita e a ordem de cores só são aplicados após reboot.

## O que deve obter

- Após o claim, o dispositivo é visível no portal online.
- Um comando de iluminação do portal ou aplicação acende o slot seleccionado na fita durante o tempo especificado. Um novo comando apaga o slot anterior e acende o seguinte.
- Se o SHT31 está instalado — a temperatura e humidade são actualizadas regularmente no portal.
- Se o SHT31 não está instalado — as leituras climáticas estão ausentes, isto é normal.
