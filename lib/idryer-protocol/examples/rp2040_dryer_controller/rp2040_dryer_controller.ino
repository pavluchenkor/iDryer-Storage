/**
 * @file rp2040_dryer_controller.ino
 * @brief RP2040 Dryer Controller Example
 *
 * Пример использования библиотеки idryer-protocol на RP2040
 * в роли контроллера сушилки.
 *
 * RP2040 управляет:
 * - Датчиками (температура, влажность)
 * - Нагревателями
 * - Вентиляторами
 * - RFID считывателями
 * - Весовыми датчиками
 *
 * И отправляет данные в ESP32 по UART для передачи в облако.
 *
 * Hardware:
 * - Raspberry Pi Pico / RP2040
 * - UART подключение к ESP32 (GPIO0/TX → ESP32 RX, GPIO1/RX → ESP32 TX)
 * - Датчики DHT22 / SHT31 на I2C
 * - Управление нагревателями через GPIO
 */

#include <Arduino.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace DryerUart;
using namespace idryer::hal;

// ============================================================================
// Конфигурация UART
// ============================================================================
#define UART_TX_PIN 0  // GPIO0 → ESP32 RX
#define UART_RX_PIN 1  // GPIO1 → ESP32 TX
#define UART_BAUD   115200

// ============================================================================
// Конфигурация устройства
// ============================================================================
constexpr uint8_t NUM_UNITS = 4;  // Количество сушильных юнитов
constexpr uint32_t FIRMWARE_VERSION = (1u << 16); // 1.0.0

// ============================================================================
// Глобальные объекты
// ============================================================================
ArduinoSerial uartSerial(Serial1);
UartBridge uartBridge;

// Симуляция данных сушилки (в реальном проекте - чтение с датчиков)
struct DryerUnit {
    uint8_t unitId;
    float temperature;     // °C
    uint8_t humidity;      // %
    uint8_t heaterPower;   // %
    bool fanOn;
    DryerMode mode;
    uint8_t sessionNum;
    uint32_t elapsedSeconds;
    uint32_t remainingSeconds;
    float targetTemp;
    uint16_t durationMinutes;
    uint8_t targetHumidity;
};

DryerUnit units[NUM_UNITS] = {
    {0, 25.0f, 50, 0, false, DryerMode::Idle, 0, 0, 0, 0.0f, 0, 0},
    {1, 25.0f, 50, 0, false, DryerMode::Idle, 0, 0, 0, 0.0f, 0, 0},
    {2, 25.0f, 50, 0, false, DryerMode::Idle, 0, 0, 0, 0.0f, 0, 0},
    {3, 25.0f, 50, 0, false, DryerMode::Idle, 0, 0, 0, 0.0f, 0, 0}
};

uint32_t lastTelemetrySent = 0;
uint32_t lastStatusSent = 0;
uint32_t lastHeartbeatSent = 0;
uint32_t bootTime = 0;

// Прототипы
void sendStatus();

// ============================================================================
// Обработчики команд от ESP32
// ============================================================================

void handleCommand(const CommandPayload& payload, const FrameHeader& header) {
    Serial.printf("\n[CMD] Received command: code=%d, state=%d, arg0=%d, arg1=%u\n",
                  payload.command, payload.targetState, payload.arg0, payload.arg1);

    uint8_t unitId = payload.unitId; // 0-3 или 0xFF (все юниты)
    if (unitId >= NUM_UNITS) {
        Serial.printf("[CMD] Invalid unitId: %d\n", unitId);
        uartBridge.sendCommandAck(header.sequence, ErrorCode::InvalidPayload);
        return;
    }

    DryerUnit& unit = units[unitId];

    // Обрабатываем команду
    switch (payload.command) {
        case CommandCode::Start:
            unit.mode = DryerMode::Drying;
            unit.sessionNum++;
            unit.targetTemp = payload.arg0 / 10.0f;  // arg0 в десятых долях °C
            unit.durationMinutes = payload.arg1;
            unit.elapsedSeconds = 0;
            unit.remainingSeconds = payload.arg1 * 60;
            unit.heaterPower = 80;
            unit.fanOn = true;
            Serial.printf("[CMD] Unit %d started: target=%.1f°C, duration=%umin\n",
                         unitId, unit.targetTemp, unit.durationMinutes);
            break;

        case CommandCode::Stop:
            unit.mode = DryerMode::Idle;
            unit.heaterPower = 0;
            unit.fanOn = false;
            unit.elapsedSeconds = 0;
            unit.remainingSeconds = 0;
            Serial.printf("[CMD] Unit %d stopped\n", unitId);
            break;

        default:
            Serial.printf("[CMD] Unknown command: %d\n", payload.command);
            break;
    }

    // Отправляем ACK (sequence, status)
    uartBridge.sendCommandAck(header.sequence, ErrorCode::None);

    // Отправляем обновленный статус
    sendStatus();
}

void handleConfig(const ConfigPayload& payload, const FrameHeader& header) {
    Serial.printf("\n[CFG] Received config: temp=%d, humidity=%u, duration=%u, fan=%u\n",
                  payload.targetTemperatureC10, payload.targetHumidityPct,
                  payload.durationMinutes, payload.fanDutyPct);

    // Применяем конфигурацию ко всем юнитам
    for (uint8_t i = 0; i < NUM_UNITS; i++) {
        units[i].targetTemp = payload.targetTemperatureC10 / 10.0f;
        units[i].targetHumidity = payload.targetHumidityPct;
        units[i].durationMinutes = payload.durationMinutes;
    }

    // Отправляем ACK (sequence, status)
    uartBridge.sendConfigAck(header.sequence, ErrorCode::None);
}

void handleHello(const HelloPayload& payload, const FrameHeader& header) {
    Serial.printf("\n[HELLO] ESP32 connected: role=%d, fw=0x%08x\n",
                  payload.role, payload.firmwareVersion);

    // Отправляем HelloAck
    HelloAckPayload ack{};
    ack.ipAddress = 0;
    ack.ssid[0] = '\0';
    uartBridge.sendHelloAck(ack);
}

void handleHeartbeat(const HeartbeatPayload& payload, const FrameHeader& header) {
    // ESP32 отправляет heartbeat, обновляем сторожевой таймер
    Serial.printf("[HB] ESP32 alive: uptime=%us, rssi=%ddBm\n",
                  payload.uptimeSeconds, payload.wifiRssiDbm);
}

void handleError(const ErrorPayload& payload, bool remote) {
    Serial.printf("\n[ERROR] %s error: code=%d, seq=%d, detail=%d\n",
                  remote ? "Remote" : "Local",
                  (int)payload.code,
                  payload.lastSequence,
                  payload.detail);
}

// ============================================================================
// Отправка данных в ESP32
// ============================================================================

void sendTelemetry() {
    TelemetryPayload payload{};
    payload.count = NUM_UNITS;

    for (uint8_t i = 0; i < NUM_UNITS; i++) {
        auto& entry = payload.units[i];
        entry.unitId = units[i].unitId;
        entry.temperatureC10 = static_cast<int16_t>(units[i].temperature * 10);
        entry.humidityPct10 = units[i].humidity * 10;
        entry.heaterPowerPct = units[i].heaterPower;
        entry.fanOn = units[i].fanOn ? 1 : 0;
    }

    uartBridge.sendTelemetry(payload);
    Serial.printf("[TX] Telemetry sent (%d units)\n", payload.count);
}

void sendStatus() {
    StatusPayload payload{};
    payload.uptime = (millis() - bootTime) / 1000;
    payload.count = NUM_UNITS;

    for (uint8_t i = 0; i < NUM_UNITS; i++) {
        auto& entry = payload.units[i];
        entry.unitId = units[i].unitId;
        entry.mode = units[i].mode;
        entry.sessionNum = units[i].sessionNum;
        entry.targetTempC10 = static_cast<int16_t>(units[i].targetTemp * 10);
        entry.targetHumidityPct = units[i].targetHumidity;
        entry.durationMinutes = units[i].durationMinutes;
        entry.elapsedSeconds = units[i].elapsedSeconds;
        entry.totalRemainingSeconds = units[i].remainingSeconds;
        entry.currentStage = 0;
        entry.totalStages = 0;
        entry.stageElapsedSeconds = 0;
        entry.stageRemainingSeconds = 0;
    }

    uartBridge.sendStatus(payload);
    Serial.printf("[TX] Status sent (uptime=%us)\n", payload.uptime);
}

void sendHeartbeat() {
    HeartbeatPayload payload{};
    payload.uptimeSeconds = (millis() - bootTime) / 1000;
    payload.wifiRssiDbm = 0;  // RP2040 не имеет WiFi
    payload.errorsSinceBoot = 0;

    uartBridge.sendHeartbeat(payload);
}

void sendInitialHello() {
    HelloPayload payload{};
    payload.role = Role::RpController;
    payload.firmwareVersion = FIRMWARE_VERSION;
    payload.workTimeCounter = (millis() - bootTime) / 1000;
    strncpy(payload.hardwareVersion, "RP2040-v1.0", sizeof(payload.hardwareVersion) - 1);

    uartBridge.sendHello(payload, false);
    Serial.println("[TX] Initial Hello sent to ESP32");
}

// ============================================================================
// Симуляция работы сушилки
// ============================================================================

void updateDryerSimulation() {
    for (uint8_t i = 0; i < NUM_UNITS; i++) {
        DryerUnit& unit = units[i];

        if (unit.mode == DryerMode::Drying) {
            // Симулируем нагрев
            if (unit.temperature < unit.targetTemp) {
                unit.temperature += 0.5f;  // Нагрев
            }

            // Симулируем снижение влажности
            if (unit.humidity > unit.targetHumidity) {
                unit.humidity -= 1;
            }

            // Обновляем таймеры (каждую секунду)
            static uint32_t lastUpdate = 0;
            if (millis() - lastUpdate >= 1000) {
                unit.elapsedSeconds++;
                if (unit.remainingSeconds > 0) {
                    unit.remainingSeconds--;
                }

                // Завершаем цикл если время вышло
                if (unit.remainingSeconds == 0) {
                    unit.mode = DryerMode::Idle;
                    unit.heaterPower = 0;
                    unit.fanOn = false;
                    Serial.printf("[DRYER] Unit %d finished drying cycle\n", i);
                }

                lastUpdate = millis();
            }
        } else {
            // В режиме IDLE постепенно остываем
            if (unit.temperature > 25.0f) {
                unit.temperature -= 0.1f;
            }
        }
    }
}

// ============================================================================
// Setup & Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("RP2040 Dryer Controller - idryer-protocol");
    Serial.println("========================================\n");

    // Инициализация HAL (логирование через Serial)
    initArduinoHal(&Serial);

    // Инициализация UART для связи с ESP32
    Serial1.setTX(UART_TX_PIN);
    Serial1.setRX(UART_RX_PIN);
    Serial1.begin(UART_BAUD);

    Serial.printf("[INIT] UART initialized (TX=%d, RX=%d, %d baud)\n",
                  UART_TX_PIN, UART_RX_PIN, UART_BAUD);

    // Инициализация UartBridge (через HAL обёртку)
    uartBridge.begin(&uartSerial, UART_BAUD);

    // Регистрация обработчиков
    uartBridge.setCommandHandler(handleCommand);
    uartBridge.setConfigHandler(handleConfig);
    uartBridge.setHelloHandler(handleHello);
    uartBridge.setHeartbeatHandler(handleHeartbeat);
    uartBridge.setErrorHandler(handleError);

    Serial.println("[INIT] UartBridge configured");

    bootTime = millis();

    // Отправляем начальное приветствие
    delay(1000);  // Даем ESP32 время инициализироваться
    sendInitialHello();

    Serial.println("[INIT] Ready to communicate with ESP32\n");
}

void loop() {
    // Обработка входящих UART сообщений от ESP32
    uartBridge.loop();

    // Обновление симуляции работы сушилки
    updateDryerSimulation();

    uint32_t now = millis();

    // Отправка телеметрии каждые 5 секунд
    if (now - lastTelemetrySent >= 5000) {
        sendTelemetry();
        lastTelemetrySent = now;
    }

    // Отправка статуса каждые 10 секунд
    if (now - lastStatusSent >= 10000) {
        sendStatus();
        lastStatusSent = now;
    }

    // Отправка heartbeat каждые 5 секунд (HEARTBEAT_INTERVAL_MS)
    if (now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
        lastHeartbeatSent = now;
    }
}
