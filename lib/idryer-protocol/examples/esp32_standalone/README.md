# ESP32 Standalone Example

ESP32 как самостоятельное iDryer-устройство (Single-MCU, без RP2040/UART).

## Описание

Полный цикл подключения к облаку iDryer на одном ESP32:

1. **WiFi** — подключение к точке доступа
2. **Provision** — получение токена от Backend по serial (MAC-адрес)
3. **Claim** — отображение PIN-кода, ожидание привязки через приложение
4. **MQTT** — публикация telemetry/status, приём команд

## Когда использовать

- ESP32 управляет сушилкой напрямую (без отдельного контроллера)
- Прототипирование и тестирование облачного API
- Standalone IoT-устройства на базе idryer-protocol

## Настройка

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD  = "YOUR_WIFI_PASSWORD";
const char* API_BASE_URL   = "https://api.idryer.io";
```

## Использование

1. Отредактируйте WiFi credentials
2. Загрузите скетч на ESP32
3. Откройте Serial Monitor (115200 бод)
4. Дождитесь PIN-кода
5. Введите PIN в приложении iDryer
6. Устройство подключится к MQTT и начнёт публиковать данные

## Интерактивные команды

В Serial Monitor можно ввести:
- `claim` — запустить процесс привязки (получить PIN)

## Ожидаемый вывод

```
========================================
 iDryer ESP32 Standalone Example
========================================

[INIT] Serial: AABBCCDDEEFF
[INIT] Waiting for WiFi...

[CLOUD] Idle -> WifiConnecting
[CLOUD] WifiConnecting -> Provisioning
[CLOUD] Device NOT claimed. Press BOOT button or send 'claim' to Serial.

> claim
[USER] Requesting claim...

========================================
  PIN: 12345678  (expires in 600s)
  Enter this PIN in the iDryer app
========================================

[CLOUD] Provisioning -> AwaitingClaim
[CLOUD] Claimed! deviceId=abc-123-def
[CLOUD] AwaitingClaim -> Ready
[CLOUD] Ready -> MqttConnecting
[CLOUD] MqttConnecting -> Online
```

## Отличие от двух-MCU конфигурации

| | Single-MCU (этот пример) | Two-MCU (ESP32 + RP2040) |
|---|---|---|
| Serial Number | MAC-адрес ESP32 | ID контроллера RP2040 (по UART) |
| `waitForMcuSerial` | `false` | `true` (ждёт Hello) |
| UART | Не используется | ESP32 ↔ RP2040 |
| Телеметрия | Генерируется на ESP32 | Приходит от RP2040 |

## Требования

- ESP32 с WiFi
- Arduino framework (PlatformIO или Arduino IDE)
- Доступ к Backend API (`api.idryer.io`)
