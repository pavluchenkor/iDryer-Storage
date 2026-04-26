/**
 * @file main.cpp
 * @brief Главный файл iHeater Link (ESP32-C3)
 *
 * iHeater Link — ESP32 без UART-компаньона. Обеспечивает:
 * - WiFi (через Improv Wi-Fi по Serial)
 * - MQTT с backend iDryer
 * - Claim (через WebSerial install.idryer.org)
 * - Локальное применение команд портала на pulse output (STM32 iHeater)
 * - Интеграцию с Bambu Lab (local MQTT) для автоматики нагрева
 */

#include <Arduino.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>
#include <ArduinoJson.h>
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#include <menu_commands.h>
#include <menu_cache.h>
#include <menu_meta.h>

#include "iheater/HeaterDevice.h"
#include "secrets.h"
#include "version.h"

using namespace idryer;
using namespace idryer::hal;

#define ANSI_RESET "\033[0m"
#define ANSI_GREEN "\033[32m"

#define DEBUG_LOG(...)                  \
    do                                  \
    {                                   \
        if (logsEnabled)                \
            Serial.printf(__VA_ARGS__); \
    } while (0)

namespace
{
    // Improv Wi-Fi
    Preferences preferences;
    ImprovWiFi improvSerial(&Serial);
    bool wifiConfigured = false;
    bool logsEnabled = false; // Wi-Fi уже подключён и Serial отдан под логи

    // Сохранить SSID/пароль в NVS (Preferences namespace "wifi").
    void saveWiFiCredentials(const char *ssid, const char *password)
    {
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.putBool("configured", true);
        preferences.end();
        DEBUG_LOG("[IMPROV] WiFi credentials saved\n");
    }

    // Прочитать SSID/пароль из NVS. Возвращает false если ничего не сохранено.
    bool loadWiFiCredentials(String &ssid, String &password)
    {
        preferences.begin("wifi", true);
        bool configured = preferences.getBool("configured", false);
        if (configured)
        {
            ssid = preferences.getString("ssid", "");
            password = preferences.getString("password", "");
        }
        preferences.end();
        return configured && ssid.length() > 0;
    }

    // =============================================================================
    // ГЛОБАЛЬНЫЕ ОБЪЕКТЫ
    // =============================================================================

    ArduinoWifiManager wifiManager;
    ArduinoHttpClient httpClient;
    ArduinoCredentialStore credStore;

    iheaterlink::HeaterDevice device(&wifiManager, &httpClient, &credStore, IDRYER_API_BASE);

    // Improv: пользователь ввёл credentials — сохранить в NVS и поднять Wi-Fi.
    void onImprovWiFiConnectCallback(const char *ssid, const char *password)
    {
        DEBUG_LOG("[IMPROV] Received credentials - SSID: %s\n", ssid);
        saveWiFiCredentials(ssid, password);
        wifiManager.begin(ssid, password);
        wifiConfigured = true;
    }

    // Improv: ошибка при настройке Wi-Fi — только лог.
    void onImprovWiFiErrorCallback(ImprovTypes::Error err)
    {
        DEBUG_LOG("[IMPROV] Error: %d\n", err);
    }

    // =============================================================================
    // WEBSERIAL CLAIMING
    // =============================================================================

    char currentClaimPin[10] = "";
    uint32_t claimPinExpiresIn = 0;

    // Claim PIN от CloudStateMachine: отправить в Serial для flasher-portal и вывести баннер.
    void onWebClaimPin(const char *pin, uint32_t expiresInSeconds)
    {
        strncpy(currentClaimPin, pin, sizeof(currentClaimPin) - 1);
        currentClaimPin[sizeof(currentClaimPin) - 1] = '\0';
        claimPinExpiresIn = expiresInSeconds;

        // Machine-readable — для flasher-portal (Web Serial).
        Serial.print("CLAIM_PIN:");
        Serial.print(pin);
        Serial.print(":");
        Serial.println(expiresInSeconds);

        // Human-readable баннер — для dev-клэйма через Serial Monitor.
        Serial.println();
        Serial.println("================================");
        Serial.printf("  PIN: %s\n", pin);
        Serial.println("  Введите в приложении iDryer");
        Serial.printf("  Действителен: %u сек\n", expiresInSeconds);
        Serial.println("================================");
        Serial.println();
        Serial.flush();

        DEBUG_LOG("[CLAIM] PIN sent to Serial: %s (expires in %ds)\n", pin, expiresInSeconds);
    }

    /// Триггерит claim и печатает машинно-читаемый ответ (для flasher-portal).
    /// Используется и для START_CLAIM, и для `claim`.
    void triggerClaim()
    {
        bool result = device.requestClaimProcess();

        if (result)
        {
            auto *csm = device.getCloudStateMachine();
            if (csm && csm->getState() == cloud::CloudState::Ready)
            {
                const char *serial = csm->getIdentity().serialNumber;
                Serial.printf("CLAIM_ALREADY:%s\n", serial);
            }
            else
            {
                Serial.println("CLAIM_STARTED:OK");
            }
        }
        else
        {
            Serial.println("CLAIM_STARTED:ERROR");
        }
        Serial.flush();
    }

    void handleWebSerialCommand(const String &line)
    {
        // START_CLAIM — от flasher-portal (Web Serial).
        // claim     — через Serial Monitor (dev-путь).
        if (line.equalsIgnoreCase("START_CLAIM") || line.equalsIgnoreCase("claim"))
        {
            DEBUG_LOG("[CLAIM] Received '%s' command\n", line.c_str());
            triggerClaim();
        }
        else if (line.equalsIgnoreCase("rmt_sweep"))
        {
            DEBUG_LOG("[TEST] RMT sweep start\n");
            device.startRmtSweep();
        }
        else if (line.equalsIgnoreCase("rmt_stop"))
        {
            DEBUG_LOG("[TEST] RMT sweep stop\n");
            device.stopRmtSweep();
        }
    }

    // Читать строку из Serial и передать в handleWebSerialCommand (только если logsEnabled).
    void processWebSerialCommands()
    {
        if (!logsEnabled)
            return;

        if (Serial.available() > 0)
        {
            String line = Serial.readStringUntil('\n');
            line.trim();

            if (line.length() > 0)
            {
                handleWebSerialCommand(line);
            }
        }
    }

} // namespace

void setup()
{
    Serial.begin(115200);

    // Improv занимает Serial до первого успешного коннекта — HAL логи пока в /dev/null.
    initArduinoHal(nullptr);

    improvSerial.setDeviceInfo(
        ImprovTypes::ChipFamily::CF_ESP32_C3,
        "iHeater Link",
        VERSION_STR,
        "iHeater Link",
        "");

    improvSerial.onImprovConnected(onImprovWiFiConnectCallback);
    improvSerial.onImprovError(onImprovWiFiErrorCallback);

    String savedSSID, savedPassword;
    if (loadWiFiCredentials(savedSSID, savedPassword))
    {
        wifiManager.begin(savedSSID.c_str(), savedPassword.c_str());
        wifiConfigured = true;
    }
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    else
    {
        wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
        saveWiFiCredentials(WIFI_SSID, WIFI_PASSWORD);
        wifiConfigured = true;
    }
#endif

    device.begin();
    device.setClaimPinCallback(onWebClaimPin);
}

void loop()
{
    device.loop();

    if (!logsEnabled)
    {
        improvSerial.handleSerial();

        if (wifiConfigured && WiFi.status() == WL_CONNECTED)
        {
            logsEnabled = true;
            initArduinoHal(&Serial);

            // mDNS нужен для резолва homeassistant.local (HA-интеграция).
            // В оригинальном iDryer его запускает WsServer — здесь его нет.
            {
                const auto &id = device.getIdentity();
                const char *mdnsName = id.hasSerialNumber() ? id.serialNumber : "iheater-link";
                MDNS.begin(mdnsName);
                MDNS.addService("_idryer", "_tcp", 80);
            }

            Serial.println("\n========================================");
            Serial.printf("[BOOT] iHeater Link FW=%s\n", VERSION_STR);
            Serial.println("[BOOT] Logs enabled after WiFi config");
            // Диагностика: что реально прочитано из NVS в момент boot
            // (store_->load() был вызван в device.begin() до включения логгера).
            {
                const auto &id = device.getIdentity();
                Serial.printf("[BOOT] NVS identity: serial=%s token=%s deviceId=%s\n",
                              id.hasSerialNumber() ? id.serialNumber : "(none)",
                              id.hasToken() ? "YES" : "no",
                              id.hasDeviceId() ? id.deviceId : "(none)");
            }
            Serial.println("[BOOT] Dev-claim: type 'claim' in Serial Monitor to trigger");
            Serial.println("========================================");
            HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm",
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());

            // Dev-автоклэйм: если в NVS нет токена — запускаем claim сразу.
            auto *csm = device.getCloudStateMachine();
            if (csm && !csm->getIdentity().hasToken())
            {
                Serial.println("[DEV] No token in NVS — auto claim");
                Serial.flush();
                triggerClaim();
            }
        }
    }
    else
    {
        processWebSerialCommands();
    }
}
