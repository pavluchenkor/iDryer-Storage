/**
 * @file esp32_standalone.ino
 * @brief ESP32 как самостоятельное iDryer-устройство (Single-MCU, без RP2040)
 *
 * Демонстрирует полный цикл подключения к облаку iDryer:
 * - WiFi → Provision → Claim (PIN) → MQTT → Online
 * - Публикация telemetry/status
 * - Приём команд от Backend
 *
 * В этой конфигурации ESP32 является одновременно контроллером сушилки
 * и облачным мостом. UART не используется — serial берётся из MAC-адреса.
 *
 * Настройте WiFi credentials и API URL, загрузите скетч,
 * откройте Serial Monitor и следите за PIN-кодом для claim.
 */

#include <WiFi.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>

using namespace idryer;
using namespace idryer::cloud;

// =============================================================================
// Настройки
// =============================================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD  = "YOUR_WIFI_PASSWORD";
const char* API_BASE_URL   = "https://api.idryer.io";

// =============================================================================
// Платформенные реализации
// =============================================================================
ArduinoWifiManager  wifiMgr;
ArduinoHttpClient   httpClient;
ArduinoCredentialStore credStore;

// Cloud компоненты
HttpApi             api(&httpClient, API_BASE_URL);
MqttClient          mqtt;
CloudStateMachine   cloud(&wifiMgr, &credStore, &api, &mqtt);

// Тайминги публикаций
uint32_t lastTelemetryMs = 0;
uint32_t lastStatusMs    = 0;

// =============================================================================
// Получение serial из MAC-адреса ESP32
// =============================================================================
void getEsp32Serial(char* buf, size_t bufSize) {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  snprintf(buf, bufSize, "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// =============================================================================
// Cloud callbacks
// =============================================================================

void onStateChange(CloudState oldState, CloudState newState, void*) {
  Serial.printf("[CLOUD] %s -> %s\n",
                cloudStateToString(oldState),
                cloudStateToString(newState));
}

void onClaimPin(const char* pin, uint32_t expiresIn, void*) {
  Serial.println();
  Serial.println("========================================");
  Serial.printf("  PIN: %s  (expires in %us)\n", pin, expiresIn);
  Serial.println("  Enter this PIN in the iDryer app");
  Serial.println("========================================");
  Serial.println();
}

void onClaimComplete(const char* deviceId, void*) {
  Serial.printf("[CLOUD] Claimed! deviceId=%s\n", deviceId);
}

void onUnclaimed(void*) {
  Serial.println("[CLOUD] Device not claimed. Press BOOT button or send 'claim' to Serial.");
}

// =============================================================================
// MQTT команды от Backend
// =============================================================================

void onMqttCommand(const char* command, JsonObjectConst data) {
  Serial.printf("[MQTT] Command: %s\n", command);
  serializeJsonPretty(data, Serial);
  Serial.println();

  if (strcmp(command, "start") == 0) {
    uint8_t unitId = data["unitId"] | 0;
    int temp       = data["params"]["temperature"] | 50;
    int duration   = data["params"]["duration"] | 60;
    Serial.printf("  -> Start unit %d: %d°C for %d min\n", unitId, temp, duration);
  }
  else if (strcmp(command, "stop") == 0) {
    uint8_t unitId = data["unitId"] | 0;
    Serial.printf("  -> Stop unit %d\n", unitId);
  }
}

// =============================================================================
// Публикация данных
// =============================================================================

void publishTelemetry() {
  StaticJsonDocument<256> doc;
  char ts[32];
  doc["timestamp"] = MqttClient::getIsoTimestamp(ts);

  JsonArray units = doc.createNestedArray("units");
  JsonObject u = units.createNestedObject();
  u["unitId"]      = 0;
  u["temperature"] = 25.0 + random(0, 50) / 10.0;
  u["humidity"]    = 40 + random(0, 20);
  u["heaterPower"] = 0;
  u["fan"]         = false;

  mqtt.publishTelemetry(doc);
}

void publishStatus() {
  StaticJsonDocument<256> doc;
  char ts[32];
  doc["timestamp"] = MqttClient::getIsoTimestamp(ts);
  doc["uptime"]    = millis() / 1000;

  JsonArray units = doc.createNestedArray("units");
  JsonObject u = units.createNestedObject();
  u["unitId"] = 0;
  u["mode"]   = "idle";

  mqtt.publishStatus(doc);
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println(" iDryer ESP32 Standalone Example");
  Serial.println("========================================\n");

  // Инициализация HAL
  initArduinoHal(&Serial);

  // WiFi
  wifiMgr.begin(WIFI_SSID, WIFI_PASSWORD);

  // Cloud callbacks
  cloud.setStateChangeCallback(onStateChange, nullptr);
  cloud.setClaimPinCallback(onClaimPin, nullptr);
  cloud.setClaimCompleteCallback(onClaimComplete, nullptr);
  cloud.setUnclaimedCallback(onUnclaimed, nullptr);

  // MQTT commands
  mqtt.setCommandCallback(onMqttCommand);

  // Single-MCU: UART отсутствует, serial gate не нужен
  cloud.setWaitForMcuSerial(false);

  // Запускаем state machine (загружает NVS, стартует WiFi)
  cloud.begin();

  // Устанавливаем serial из MAC-адреса ESP32.
  // В двух-MCU конфигурации serial приходит от RP2040 через UART Hello.
  // Здесь ESP32 сам является устройством — используем его MAC.
  char serial[13];
  getEsp32Serial(serial, sizeof(serial));
  cloud.setMcuSerial(serial);

  Serial.printf("[INIT] Serial: %s\n", serial);
  Serial.println("[INIT] Waiting for WiFi...\n");
}

// =============================================================================
// Loop
// =============================================================================

void loop() {
  cloud.loop();

  // Интерактивный claim через Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "claim") {
      Serial.println("[USER] Requesting claim...");
      cloud.requestClaim();
    }
  }

  // Публикации — только когда онлайн
  if (!cloud.isOnline())
    return;

  uint32_t now = millis();

  if (now - lastTelemetryMs >= 5000) {
    publishTelemetry();
    lastTelemetryMs = now;
  }

  if (now - lastStatusMs >= 10000) {
    publishStatus();
    lastStatusMs = now;
  }
}
