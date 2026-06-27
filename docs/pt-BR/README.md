# Storage Link — Guia rápido

Storage Link é um módulo baseado em ESP32 que transforma uma fita LED endereçável em um indicador de armazenamento de bobinas de filamento e, opcionalmente, publica temperatura e umidade do sensor SHT31.

- Conecta-se a Wi-Fi e vincula o dispositivo a [portal.idryer.org](https://portal.idryer.org/).
- Por comando da nuvem ou aplicativo local, ilumina o slot da bobina pela cor e tempo especificados.
- Com o sensor SHT31 instalado, publica temperatura e umidade.

O conhecimento sobre o armazenamento e as bobinas reside em um aplicativo externo. O firmware funciona como um módulo executor simples: "ilumine o slot N com a cor C por T segundos". Isto permite colar a fita em qualquer prateleira e descrevê-la no portal independentemente do firmware.

![Rack](../img/Rack.png)

## Placas suportadas

| Placa                        |       |
| ---------------------------- | ----- |
| ESP32-C3 DevKitM-1           | `✅`  |
| ESP32-C3 Super Mini          | `✅`  |
| Seeed XIAO ESP32-S3          | `✅`  |
| Waveshare ESP32-S3-Zero      | `✅`  |

Qualquer outra placa baseada em ESP32-C3 ou ESP32-S3 pode ser usada se houver GPIO livre para os dados da fita e um par de GPIO para I2C. Consulte o pinout do fabricante.

## Diagrama de conexão

!!! warning "Nunca conecte ou desconecte cabos com alimentação ativa."

Storage Link controla a fita por um GPIO de sinal único (`DATA`) e opcionalmente lê o SHT31 via I2C.

![wiring diagram](../img/wiring-diagram.png)

### Alimentação da fita

A fita e o ESP devem ser alimentados por uma fonte adequada para a corrente real da carga da fita.

- Em muitas placas ESP, o pino `5V` (`VBUS`) é ligado diretamente do conector USB. Se o bloco de alimentação USB usado fornecer corrente suficiente para a carga da fita, é permitido alimentar o ESP e a fita em paralelo dele.
- Se não houver margem de corrente — a alimentação da fita é usada de uma fonte 5 V separada. O negativo da fonte deve ser conectado ao `GND` do ESP — sem terra comum, o sinal `DATA` não funcionará.

Em ambos os casos, no menu `psu_ma` você deve inserir a corrente que sua fonte de alimentação 5 V realmente pode fornecer. Este é o valor especificado pelo fabricante, não um valor desejado. FastLED usará este valor para limitar o brilho total e não ultrapassar o limite.

### Boas práticas de montagem

Estes elementos não são obrigatórios para iniciar, mas eliminam problemas típicos com fitas endereçáveis (pixels faltando, "falhas" no primeiro LED, quedas de tensão ao ligar).

- **Resistor na linha `DATA`.** Um resistor de `300–500 Ohm` (tipicamente `390 Ohm`) é colocado em série entre o GPIO do ESP e `DIN` da fita, o mais próximo possível da fita. Elimina reflexões de sinal e protege o primeiro chip da fita.
- **Eletrólito na alimentação.** Entre `+5V` e `GND` na entrada de alimentação da fita — `1000 µF` em `16 V` (também `25 V` é bom, `10 V` é o mínimo). Suaviza picos de corrente em ligas rápidas.
- **Seção da terra comum de acordo com a corrente da fonte.** O cabo de terra do ESP—fita—fonte deve ser dimensionado para a corrente de pico da fita. Orientação para cabos curtos (até ~1 m):

    | Corrente da fonte | Seção           | AWG       |
    | --------------- | --------------- | --------- |
    | até 3 A         | `0,5 mm²`       | `AWG 20`  |
    | até 5 A         | `0,75 mm²`      | `AWG 18`  |

    Para a linha `+5V` para a fita — as mesmas seções. Em fitas longas, forneça alimentação em ambas as extremidades.

### Conexões de sinais

Os valores de GPIO dependem da placa.

#### ESP32-C3 DevKitM-1 e ESP32-C3 Super Mini

| ESP        | Função                      |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` da fita endereçável  |
| `GPIO8`    | `SDA` (SHT31, opcional)     |
| `GPIO9`    | `SCL` (SHT31, opcional)     |
| `GND`      | terra comum com fita e fonte |

#### Seeed XIAO ESP32-S3

| ESP        | Função                      |
| ---------- | --------------------------- |
| `GPIO2`    | `DATA` da fita endereçável  |
| `GPIO5`    | `SDA` (SHT31, opcional)     |
| `GPIO6`    | `SCL` (SHT31, opcional)     |
| `GND`      | terra comum com fita e fonte |

#### Waveshare ESP32-S3 Zero

| ESP        | Função                      |
| ---------- | --------------------------- |
| `GPIO4`    | `DATA` da fita endereçável  |
| `GPIO8`    | `SDA` (SHT31, opcional)     |
| `GPIO9`    | `SCL` (SHT31, opcional)     |
| `GND`      | terra comum com fita e fonte |

### Pinout das placas

ESP32-C3 Super Mini:

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

Waveshare ESP32-S3-Zero:

![Pinout Waveshare ESP32-S3-Zero](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

## Sensor SHT31 opcional

O sensor só é necessário se você deseja publicar temperatura e umidade neste dispositivo. Storage Link inicia e funciona com a fita da mesma forma — com ou sem o sensor. Se o sensor não estiver instalado, a temperatura e a umidade simplesmente não são enviadas.

- Barramento: I2C em `SDA`/`SCL` da placa correspondente.
- Endereço: `0x44` ou `0x45` (o firmware determinará automaticamente na inicialização).

![SH31](../img/SHT31.png)

## Gravação via flash web

O flash web está em [install.idryer.org](https://install.idryer.org/).

1. Conecte o Storage Link à porta USB do computador.
2. Abra [install.idryer.org](https://install.idryer.org/) e clique no botão **Storage Link**.
3. Selecione sua variante de placa.
4. Clique em **Connect** e selecione a porta serial. Se o dispositivo não for detectado, segure o botão `BOOT` na placa e pressione `RST` brevemente.
5. Clique em **Install**. O flash gravará o firmware.
6. Após a conclusão da gravação, o assistente de configuração do Wi-Fi será aberto.

## Configuração Wi-Fi

Após a gravação, o assistente Improv é aberto automaticamente na porta serial.

1. Insira o SSID e a senha de sua rede 2,4 GHz.
2. Aguarde o status **Connected**.

Se o assistente não abrir, desconecte o USB e conecte novamente através de **Connect** sem reprogramar.

!!! note "ESP32-C3 e ESP32-S3 suportam apenas Wi-Fi 2,4 GHz. Redes 5 GHz não funcionam."

## Vinculação ao portal

1. Na página do flash, clique em **Conectar e executar Claim**. O comando claim será enviado ao dispositivo.
2. Após alguns segundos, um PIN aparecerá na página. O PIN é válido por cerca de 5 minutos.
3. Abra [portal.idryer.org](https://portal.idryer.org/) → **Adicionar dispositivo** → insira o PIN.
4. Após a vinculação bem-sucedida, o dispositivo aparecerá na lista online.

Se o PIN não aparecer ou a vinculação não funcionar — repita o claim ou remova o dispositivo no portal e tente novamente.

## Configuração da fita

Os parâmetros são definidos através do menu de configuração do dispositivo. Alguns são aplicados imediatamente, outros apenas após a reinicialização.

| Parâmetro        | Valores                               | Padrão   | Aplicação      |
| ---------------- | ------------------------------------- | -------- | -------------- |
| `led_count`      | `1..300`, passo `1`                   | `120`    | imediatamente  |
| `psu_ma`         | `500..20000` mA, passo `100`          | `5000`   | imediatamente  |
| tipo de fita     | seleção disponível no menu            | `WS2812B`| após reboot    |
| ordem das cores  | `GRB`, `RGB`, `BRG`, `BGR`            | `GRB`    | após reboot    |
| `language`       | `ru` / `en`                           | `en`     | imediatamente  |

Lista de verificação básica após o primeiro iniciar:

1. Defina `led_count` — o número real de pixels na fita.
2. Defina `psu_ma` — a corrente nominal da fonte 5 V em miliamperes.
3. Selecione o tipo de fita que você tem instalado.
4. Selecione a ordem das cores. O padrão é `GRB`. Se o vermelho e o verde estão trocados ou a cor está incorreta — experimente as opções.
5. Reinicie o dispositivo — o tipo de fita e a ordem das cores são aplicados apenas após reboot.

## O que você deve obter

- Após claim, o dispositivo fica visível no portal online.
- Comando de iluminação do portal ou aplicativo acende o slot selecionado na fita pelo tempo especificado. Um novo comando apaga o slot anterior e acende o próximo.
- Se o SHT31 estiver instalado — temperatura e umidade são atualizadas regularmente no portal.
- Se o SHT31 não estiver instalado — as medições climáticas estarão ausentes, o que é normal.
