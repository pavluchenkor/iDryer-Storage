# UART ESP32 Bridge Example

Пример ESP32 моста для связи с RP2040 через UART.

## Описание

Демонстрирует базовую инициализацию UartBridge и обработку сообщений:
- Hello / HelloAck
- Telemetry / TelemetryAck
- Heartbeat

## Подключение

```
ESP32-C3          RP2040
---------         ------
GPIO16 (RX) <---> TX
GPIO17 (TX) <---> RX
GND         <---> GND
```

## Использование

1. Загрузите скетч на ESP32
2. Откройте Serial Monitor (115200 бод)
3. Подключите RP2040
4. Наблюдайте обмен сообщениями

## Ожидаемый вывод

```
[UART] Получен Hello от RP2040
  - Role: 1
  - FW Version: 1.0.0
  - HW Version: v1.0
[UART] Heartbeat от RP2040: uptime=10s, errors=0
[UART] Получена телеметрия (2 юнитов)
  - Unit 0: T=25.5°C, H=45%, Heater=0%, Fan=OFF
  - Unit 1: T=60.0°C, H=20%, Heater=80%, Fan=ON
```
