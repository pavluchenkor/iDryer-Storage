/**
 * @file HeaterDevice.h
 * @brief Фасад iHeater Link (application layer).
 *
 * Link работает без UART-компаньона. Info и telemetry ESP формирует сам.
 * Команды портала применяются через LinkCommandSink → RmtOutputAdapter.
 * HA / Bambu / Moonraker интеграции живут в LinkIntegrationsManager (библиотека).
 * Основной источник target для нагрева — VIRTUAL_CHAMBER из Klipper (Moonraker).
 */

#pragma once

#include <functional>
#include <core/types.h>
#include <cloud/cloud_state_machine.h>
#include <cloud/telemetry_publisher.h>
#include <cloud/command_handler.h>
#include <cloud/http_api.h>
#include <cloud/link_integrations_manager.h>
#include <cloud/link_integrations_store.h>
#include <mqtt/mqtt_client.h>
#include <device/interfaces/IWifiManager.h>
#include <device/interfaces/IHttpClient.h>
#include <device/interfaces/ICredentialStore.h>

#include "controller/RmtOutputAdapter.h"
#include "iheater/LinkCommandSink.h"
#include "iheater/MenuBridge.h"

namespace iheaterlink {

/// (pin, expiresInSeconds) — наружу для WebSerial claim UI.
using ClaimPinCallback = std::function<void(const char* pin, uint32_t expiresInSeconds)>;

/**
 * @brief Координирует облачную часть iHeater Link.
 */
class HeaterDevice {
public:
    HeaterDevice(idryer::IWifiManager* wifi,
                 idryer::IHttpClient* http,
                 idryer::ICredentialStore* store,
                 const char* apiBaseUrl = nullptr);

    /// Инициализация облака, LinkIntegrations, команд.
    void begin();

    /// Главный цикл (вызывать из Arduino loop()).
    void loop();

    /// Инициирует процесс claim. Возвращает false, если claim невозможен сейчас.
    bool requestClaimProcess();

    /// Запустить тестовый sweep RMT: весь диапазон температур вверх-вниз за 5 секунд.
    void startRmtSweep();
    void stopRmtSweep();

    /// Колбэк на PIN от CloudStateMachine — прокидывается в WebSerial UI.
    void setClaimPinCallback(ClaimPinCallback cb) { userClaimPinCallback_ = std::move(cb); }

    /// Внешняя команда (например, из WebSocket/WebSerial) в тот же CommandHandler.
    void handleExternalCommand(const char* command, JsonObjectConst data);

    // --- Диагностика состояния ---
    bool isOnline() const { return cloud_.isOnline(); }
    idryer::cloud::CloudState getCloudState() const { return cloud_.getState(); }
    const idryer::DeviceIdentity& getIdentity() const { return cloud_.getIdentity(); }
    idryer::cloud::CloudStateMachine* getCloudStateMachine() { return &cloud_; }

private:
    // --- Внутренние шаги ---
    void publishInfoOnce();
    void publishLinkTelemetryIfNeeded();
    void handleMqttCommand(const char* command, JsonObjectConst data);
    void syncTimeFromBackend(const char* timestamp);

    // Колбэки от LinkIntegrationsManager
    void onVirtualChamberUpdate(const idryer::cloud::VirtualChamberData& data);
    void onBambuPrinterStatusUpdate(const idryer::cloud::BambuPrinterStatus& status);

    // Cloud callbacks (static → ctx → self)
    static void onCloudStateChange(idryer::cloud::CloudState oldState,
                                   idryer::cloud::CloudState newState,
                                   void* ctx);
    static void onClaimPin(const char* pin, uint32_t expiresInSeconds, void* ctx);
    static void onClaimComplete(const char* deviceId, void* ctx);
    static void onUnclaimed(void* ctx);

    // --- Зависимости (извне) ---
    idryer::IWifiManager* wifi_;
    idryer::IHttpClient* http_;
    idryer::ICredentialStore* store_;

    // --- Облако ---
    idryer::cloud::HttpApi api_;
    MqttClient mqtt_;
    idryer::cloud::CloudStateMachine cloud_;
    idryer::cloud::TelemetryPublisher publisher_;

    // --- LINK-интеграции (HA / Bambu / Moonraker) ---
    idryer::cloud::LinkIntegrationsStore integrationsStore_;
    idryer::cloud::LinkIntegrationsManager integrations_;

    // --- iHeater Link ядро ---
    RmtOutputAdapter controllerOutput_;
    LinkCommandSink linkSink_;
    idryer::cloud::CommandHandler cmdHandler_;
    MenuBridge menuBridge_;

    // --- Внешний callback для WebSerial ---
    ClaimPinCallback userClaimPinCallback_;

    // --- Состояние публикаций ---
    bool infoPublished_ = false;
    uint32_t lastTelemetryAt_ = 0;

    // --- RMT sweep тест ---
    void sweepStep();
    bool sweepActive_ = false;
    int  sweepIdx_    = 0;
    int  sweepDir_    = 1;
    uint32_t sweepLastMs_ = 0;
    uint32_t sweepStepMs_ = 625;
};

} // namespace iheaterlink
