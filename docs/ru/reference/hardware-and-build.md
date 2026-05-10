# Hardware и сборка

Этот документ описывает не абстрактную библиотеку, а реальные product env-ы из [`platformio.ini`](../../../platformio.ini).

## Поддерживаемые env-ы

| Env | Плата | Назначение |
|-----|-------|------------|
| `esp32c3-storage-prod` | ESP32-C3 DevKitM-1 | production |
| `esp32c3-storage-stage` | ESP32-C3 DevKitM-1 | staging |
| `esp32c3-super-mini-prod` | ESP32-C3 Super Mini | production |
| `esp32c3-super-mini-stage` | ESP32-C3 Super Mini | staging |
| `xiao-esp32s3-prod` | Seeed XIAO ESP32-S3 | production |
| `xiao-esp32s3-stage` | Seeed XIAO ESP32-S3 | staging |
| `waveshare-esp32s3-zero-prod` | Waveshare ESP32-S3 Zero | production |
| `waveshare-esp32s3-zero-stage` | Waveshare ESP32-S3 Zero | staging |

`default_envs` собирают только production-варианты.

## Пины продукта

### C3 DevKitM-1 и C3 Super Mini

- `STORAGE_LED_PIN = 4`
- `STORAGE_I2C_SDA = 8`
- `STORAGE_I2C_SCL = 9`
- `STORAGE_MAX_LEDS = 300`

### XIAO ESP32-S3

- `STORAGE_LED_PIN = 2`
- `STORAGE_I2C_SDA = 5`
- `STORAGE_I2C_SCL = 6`
- `STORAGE_MAX_LEDS = 300`

### Waveshare ESP32-S3 Zero

- `STORAGE_LED_PIN = 4`
- `STORAGE_I2C_SDA = 8`
- `STORAGE_I2C_SCL = 9`
- `STORAGE_MAX_LEDS = 300`

### APA102 / DotStar

Если выбран тип ленты `APA102`, по умолчанию используются:

- `STORAGE_DOT_DATA_PIN = 6`
- `STORAGE_DOT_CLK_PIN = 7`

Эти значения можно переопределить build-флагами, но в текущем `platformio.ini` отдельные overrides не заданы.

## Разница между production и staging

### Production

- `IDRYER_API_BASE = https://portal.idryer.org/api`
- `MQTT_BROKER = mqtt.idryer.org`
- `MQTT_PORT = 8883`
- `MQTT_USE_TLS = 1`
- post-build script: `extra_scripts/copy_firmware.py`

### Staging

- `IDRYER_API_BASE = https://staging.idryer.org/api`
- `MQTT_BROKER = staging.idryer.org`
- `MQTT_PORT = 1884`
- `MQTT_USE_TLS = 0`
- post-build script: `extra_scripts/stage_auto_claim.py`

## Команды сборки

```bash
pio run
pio run -e esp32c3-storage-prod
pio run -e esp32c3-storage-stage
pio run -e xiao-esp32s3-prod
pio run -e waveshare-esp32s3-zero-prod
```

Прошивка и upload:

```bash
pio run -e esp32c3-storage-prod -t upload
pio device monitor
```

## Что важно для понимания структуры репозитория

- `build_src_filter = +<main.cpp> +<storage/**>`: в продуктовую сборку входит только `src/main.cpp` и `src/storage/**`.
- `reference/` содержит миграционный и исторический код, но не является текущим продуктом.
- `lib/idryer-core` подключается как библиотека платформенного уровня, а не как место продуктовой логики.
