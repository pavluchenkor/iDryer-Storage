/**
 * @file cloud_state_machine.h
 * @brief Cloud lifecycle: Wi-Fi -> provision/claim -> MQTT online.
 */

#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../hal/hal_types.h"
#include "../device/interfaces/IWifiManager.h"
#include "../device/interfaces/ICredentialStore.h"
#include "../mqtt/mqtt_client.h"
#include "http_api.h"

namespace idryer {
namespace cloud {

// Состояния облачного подключения
enum class CloudState : uint8_t {
    Idle,                ///< Начальное состояние (до begin())
    WifiConnecting,      ///< Подключение к WiFi
    WaitingForMcuSerial, ///< Ожидание mcuSerial от RP2040 по UART (первый boot)
    Provisioning,        ///< Получение токена (POST /provision)
    Registering,         ///< Получение PIN (POST /register) - по запросу
    AwaitingClaim,       ///< Ожидание привязки (polling /check-claim)
    Ready,               ///< Готов к MQTT (токен + deviceId есть)
    MqttConnecting,      ///< Подключение к MQTT брокеру
    Online               ///< Работа в MQTT (полностью подключены)
};

const char* cloudStateToString(CloudState state);

// Callbacks
typedef void (*CloudStateChangeCallback)(CloudState oldState, CloudState newState, void* ctx);

typedef void (*ClaimPinCallback)(const char* pin, uint32_t expiresInSeconds, void* ctx);

typedef void (*ClaimCompleteCallback)(const char* deviceId, void* ctx);

typedef void (*UnclaimedCallback)(void* ctx);

// Тайминги retry/poll.
struct CloudConfig {
    uint32_t wifiRetryIntervalMs = IDRYER_WIFI_RETRY_INTERVAL_MS;
    uint32_t provisionRetryMs = IDRYER_PROVISION_RETRY_MS;
    uint32_t claimPollIntervalMs = IDRYER_CLAIM_POLL_INTERVAL_MS;
    uint32_t mqttRetryIntervalMs = IDRYER_MQTT_RETRY_INTERVAL_MS;
    bool waitForMcuSerial = false;  // true → MQTT не стартует до Hello от MCU (двух-MCU конфигурация)
};

/// Координирует сетевые шаги до состояния Online.
class CloudStateMachine {
public:
    CloudStateMachine(IWifiManager* wifi,
                      ICredentialStore* store,
                      HttpApi* api,
                      MqttClient* mqtt,
                      const CloudConfig& config = CloudConfig{});

    /// Загружает credentials и запускает state machine.
    void begin();

    /// Неблокирующий цикл переходов/ретраев.
    void loop();

    CloudState getState() const { return state_; }

    bool isOnline() const { return state_ == CloudState::Online; }

    /// Инициирует claiming; возвращает `false`, если он недоступен сейчас.
    bool requestClaim();

    /// Устанавливает mcuSerial от RP2040. Если ждали его — запускает Provisioning.
    void setMcuSerial(const char* mcuSerial);

    /// Включает ожидание Hello от MCU перед стартом MQTT.
    /// Автоматически вызывается из IdryerDevice::begin() при наличии UART.
    void setWaitForMcuSerial(bool wait) { config_.waitForMcuSerial = wait; }

    /// Принудительный re-provision для обновления deviceToken (при WS auth fail).
    /// Вызывает POST /devices/provision, обновляет NVS. Cooldown 30 сек.
    /// Возвращает true, если токен успешно обновлён.
    bool refreshToken();

    const DeviceIdentity& getIdentity() const { return identity_; }

    void setStateChangeCallback(CloudStateChangeCallback cb, void* ctx);
    void setClaimPinCallback(ClaimPinCallback cb, void* ctx);
    void setClaimCompleteCallback(ClaimCompleteCallback cb, void* ctx);
    void setUnclaimedCallback(UnclaimedCallback cb, void* ctx);

private:
    // Обработчики состояний.
    void handleWifiConnecting();
    void handleWaitingForMcuSerial();
    void handleProvisioning();
    void handleAwaitingClaim();
    void handleReady();
    void handleMqttConnecting();
    void handleOnline();

    // Смена состояния + нотификация callback.
    void setState(CloudState newState);

    // Dependencies
    IWifiManager* wifi_;
    ICredentialStore* store_;
    HttpApi* api_;
    MqttClient* mqtt_;
    CloudConfig config_;

    // State
    CloudState state_ = CloudState::Idle;
    DeviceIdentity identity_;

    // Timing
    uint32_t lastWifiAttempt_ = 0;
    uint32_t lastProvisionAttempt_ = 0;
    uint32_t lastTokenRefreshMs_ = 0;
    uint32_t lastClaimPoll_ = 0;
    uint32_t lastMqttAttempt_ = 0;

    // Flags
    bool awaitingClaim_ = false;
    bool mqttInitialized_ = false;
    bool unclaimedNotified_ = false;  // callback уже вызван
    bool serialVerified_ = false;     // true после первого Hello с совпадающим serial

    // Pending data
    char pendingPin_[IDRYER_MAX_PIN_LEN];

    // Callbacks
    CloudStateChangeCallback stateCallback_ = nullptr;
    void* stateCallbackCtx_ = nullptr;
    ClaimPinCallback claimPinCallback_ = nullptr;
    void* claimPinCtx_ = nullptr;
    ClaimCompleteCallback claimCompleteCallback_ = nullptr;
    void* claimCompleteCtx_ = nullptr;
    UnclaimedCallback unclaimedCallback_ = nullptr;
    void* unclaimedCtx_ = nullptr;
};

} // namespace cloud
} // namespace idryer
