/**
 * @file uart_esp32_bridge.ino
 * @brief Пример ESP32 моста для связи RP2040 ↔ Backend через UART
 *
 * Этот пример демонстрирует:
 * - Инициализацию UART bridge через ArduinoSerial (HAL)
 * - Обработку Hello сообщений от RP2040
 * - Обработку Telemetry данных
 * - Отправку Heartbeat сообщений
 *
 * Подключение:
 * - ESP32 RX (GPIO16) ↔ RP2040 TX
 * - ESP32 TX (GPIO17) ↔ RP2040 RX
 * - GND ↔ GND
 */

#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace DryerUart;
using namespace idryer::hal;

// ESP32 UART пины для связи с RP2040
const int UART_RX_PIN = 16;
const int UART_TX_PIN = 17;

// HAL обёртка над Serial1 (UART_NUM_1)
ArduinoSerial uartSerial(Serial1, 1);
UartBridge uartBridge;

uint32_t lastHeartbeatMs = 0;

// ============================================================================
// Обработчики входящих сообщений
// ============================================================================

void onHello(const HelloPayload &payload, const FrameHeader &header) {
  Serial.printf("[UART] Получен Hello от RP2040\n");
  Serial.printf("  - Role: %d\n", (int)payload.role);
  Serial.printf("  - FW Version: %d.%d.%d\n",
                (payload.firmwareVersion >> 16) & 0xFF,
                (payload.firmwareVersion >> 8) & 0xFF,
                payload.firmwareVersion & 0xFF);
  Serial.printf("  - HW Version: %s\n", payload.hardwareVersion);

  // Отправляем HelloAck
  HelloAckPayload response{};
  response.ipAddress = 0;  // нет IP (нет WiFi в этом примере)
  response.ssid[0] = '\0';
  uartBridge.sendHelloAck(response);
}

void onTelemetry(const TelemetryPayload &payload, const FrameHeader &header) {
  Serial.printf("[UART] Получена телеметрия (%d юнитов)\n", payload.count);
  for (uint8_t i = 0; i < payload.count; i++) {
    const auto &unit = payload.units[i];
    Serial.printf("  - Unit %d: T=%.1f°C, H=%.1f%%, Heater=%d%%, Fan=%s\n",
                  unit.unitId,
                  unit.temperatureC10 / 10.0,
                  unit.humidityPct10 / 10.0,
                  unit.heaterPowerPct,
                  unit.fanOn ? "ON" : "OFF");
  }

  // Отправляем ACK
  uartBridge.sendTelemetryAck(header.sequence);
}

void onHeartbeat(const HeartbeatPayload &payload, const FrameHeader &header) {
  Serial.printf("[UART] Heartbeat от RP2040: uptime=%ds, errors=%d\n",
                payload.uptimeSeconds,
                payload.errorsSinceBoot);
}

void onError(const ErrorPayload &payload, bool remote) {
  Serial.printf("[UART] %s ошибка: code=%d, seq=%d, detail=%d\n",
                remote ? "Удаленная" : "Локальная",
                (int)payload.code,
                payload.lastSequence,
                payload.detail);
}

// ============================================================================
// Setup & Loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("iDryer UART Bridge - ESP32 Example");
  Serial.println("========================================\n");

  // Инициализация HAL (логирование через Serial)
  initArduinoHal(&Serial);

  // Инициализация UART1 для связи с RP2040
  Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // Инициализация UartBridge (через HAL обёртку)
  uartBridge.begin(&uartSerial, 115200);

  // Регистрация обработчиков
  uartBridge.setHelloHandler(onHello);
  uartBridge.setTelemetryHandler(onTelemetry);
  uartBridge.setHeartbeatHandler(onHeartbeat);
  uartBridge.setErrorHandler(onError);

  // Отправляем HelloRequest — просим MCU прислать свой Hello с данными устройства.
  // MCU отвечает Hello (role=RpController) → LINK отвечает HelloAck.
  HelloPayload req{};
  req.role = Role::HelloRequest;
  uartBridge.sendHello(req);

  Serial.println("[SETUP] UART bridge инициализирован");
  Serial.println("[SETUP] Ожидание сообщений от RP2040...\n");
}

void loop() {
  // Обработка входящих UART сообщений
  uartBridge.loop();

  // Отправка Heartbeat каждые 5 секунд
  if (millis() - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    HeartbeatPayload hb{};
    hb.uptimeSeconds = millis() / 1000;
    hb.wifiRssiDbm = 0;  // нет WiFi в этом примере
    hb.errorsSinceBoot = 0;
    uartBridge.sendHeartbeat(hb);

    lastHeartbeatMs = millis();
  }
}
