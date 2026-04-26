/**
 * @file cloud_state_machine.cpp
 * @brief Реализация машины состояний облачного подключения
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "cloud_state_machine.h"
#include <stdio.h>
#include <stdlib.h>

namespace idryer
{
    namespace cloud
    {

        // =============================================================================
        // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
        // =============================================================================

        const char *cloudStateToString(CloudState state)
        {
            switch (state)
            {
            case CloudState::Idle:
                return "Idle";
            case CloudState::WifiConnecting:
                return "WifiConnecting";
            case CloudState::WaitingForMcuSerial:
                return "WaitingForMcuSerial";
            case CloudState::Provisioning:
                return "Provisioning";
            case CloudState::Registering:
                return "Registering";
            case CloudState::AwaitingClaim:
                return "AwaitingClaim";
            case CloudState::Ready:
                return "Ready";
            case CloudState::MqttConnecting:
                return "MqttConnecting";
            case CloudState::Online:
                return "Online";
            default:
                return "Unknown";
            }
        }

        // =============================================================================
        // КОНСТРУКТОР
        // =============================================================================

        CloudStateMachine::CloudStateMachine(IWifiManager *wifi,
                                             ICredentialStore *store,
                                             HttpApi *api,
                                             MqttClient *mqtt,
                                             const CloudConfig &config)
            : wifi_(wifi), store_(store), api_(api), mqtt_(mqtt), config_(config)
        {
            pendingPin_[0] = '\0';
        }

        // =============================================================================
        // ИНИЦИАЛИЗАЦИЯ
        // =============================================================================

        void CloudStateMachine::begin()
        {
            // Инициализируем хранилище
            store_->begin();

            // Загружаем сохранённые credentials
            store_->load(identity_);

            HAL_LOG_INFO("CLOUD", "Init: serial=%s deviceId=%s",
                         identity_.hasSerialNumber() ? identity_.serialNumber : "(waiting for RP2040)",
                         identity_.hasDeviceId() ? identity_.deviceId : "(none)");

            // Стартуем с подключения к WiFi
            setState(CloudState::WifiConnecting);
            lastWifiAttempt_ = HAL_MILLIS() - config_.wifiRetryIntervalMs;
        }

        // =============================================================================
        // ГЛАВНЫЙ ЦИКЛ
        // =============================================================================

        void CloudStateMachine::loop()
        {
            // Обрабатываем WiFi события
            wifi_->loop();

            // Обрабатываем текущее состояние
            switch (state_)
            {
            case CloudState::WifiConnecting:
                handleWifiConnecting();
                break;

            case CloudState::WaitingForMcuSerial:
                handleWaitingForMcuSerial();
                break;

            case CloudState::Provisioning:
                handleProvisioning();
                break;

            case CloudState::AwaitingClaim:
                handleAwaitingClaim();
                break;

            case CloudState::Ready:
                handleReady();
                break;

            case CloudState::MqttConnecting:
                handleMqttConnecting();
                break;

            case CloudState::Online:
                handleOnline();
                break;

            default:
                break;
            }
        }

        // =============================================================================
        // ОБРАБОТЧИКИ СОСТОЯНИЙ
        // =============================================================================

        void CloudStateMachine::handleWifiConnecting()
        {
            // WiFi уже подключен?
            if (wifi_->isConnected())
            {
                char ip[IDRYER_MAX_IP_LEN];
                wifi_->getLocalIP(ip, sizeof(ip));
                HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm", ip, wifi_->getRSSI());

                // Если serial ещё не известен — ждём в любом случае.
                // Если waitForMcuSerial (двух-MCU) — ждём Hello для верификации serial.
                // serialVerified_ сохраняется true в рамках сессии (WiFi reconnect без задержки).
                if (!identity_.hasSerialNumber() || (config_.waitForMcuSerial && !serialVerified_))
                {
                    setState(CloudState::WaitingForMcuSerial);
                    return;
                }

                // Serial verified — переходим к provisioning (или дальше если токен уже есть)
                if (identity_.hasToken())
                {
                    if (identity_.hasDeviceId())
                    {
                        setState(CloudState::Ready);
                    }
                    else
                    {
                        setState(CloudState::Provisioning);
                    }
                }
                else
                {
                    setState(CloudState::Provisioning);
                }
                return;
            }

            // Таймаут для retry
            const uint32_t now = HAL_MILLIS();
            if (now - lastWifiAttempt_ < config_.wifiRetryIntervalMs)
            {
                return;
            }
            lastWifiAttempt_ = now;

            HAL_LOG_INFO("CLOUD", "Connecting to WiFi...");
            wifi_->connect();
        }

        void CloudStateMachine::handleProvisioning()
        {
            // WiFi отключился?
            if (!wifi_->isConnected())
            {
                setState(CloudState::WifiConnecting);
                return;
            }

            // Токен уже есть?
            if (identity_.hasToken())
            {
                // Если deviceId тоже есть - готовы к MQTT
                if (identity_.hasDeviceId())
                {
                    setState(CloudState::Ready);
                }
                else if (!unclaimedNotified_)
                {
                    // Токен есть, но deviceId нет - устройство не привязано
                    unclaimedNotified_ = true;
                    HAL_LOG_WARN("CLOUD", "Device NOT claimed (token exists). Waiting for claim request.");
                    if (unclaimedCallback_)
                    {
                        unclaimedCallback_(unclaimedCtx_);
                    }
                }
                // Остаёмся в Provisioning (ждём requestClaim())
                return;
            }

            // Таймаут для retry
            const uint32_t now = HAL_MILLIS();
            if (now - lastProvisionAttempt_ < config_.provisionRetryMs)
            {
                return;
            }
            lastProvisionAttempt_ = now;

            HAL_LOG_INFO("CLOUD", "Provisioning device...");

            // Выполняем provision
            ProvisionResult result = api_->provision(identity_.serialNumber);

            if (!result.success)
            {
                HAL_LOG_WARN("CLOUD", "Provision failed");
                return;
            }

            // Backend вернул isClaimed=true но без token — serial привязан к другому владельцу,
            // или устройство нуждается в re-claim (удалить из приложения, затем повторить).
            if (result.isClaimed && result.token[0] == '\0')
            {
                HAL_LOG_WARN("CLOUD", "Serial %s is claimed but token withheld by server. Delete device in app and re-claim.",
                             identity_.serialNumber);
                if (unclaimedCallback_)
                {
                    unclaimedCallback_(unclaimedCtx_);
                }
                return;
            }

            // Сохраняем токен
            identity_.setToken(result.token);

            // Если устройство уже привязано - сохраняем deviceId
            if (result.isClaimed && result.deviceId[0] != '\0')
            {
                identity_.setDeviceId(result.deviceId);
            }

            // Сохраняем в хранилище
            store_->save(identity_);

            HAL_LOG_INFO("CLOUD", "Provision OK: isNew=%d isClaimed=%d",
                         result.isNew, result.isClaimed);

            // Если уже привязано - готовы к MQTT
            if (identity_.hasDeviceId())
            {
                setState(CloudState::Ready);
            }
            else
            {
                // Устройство не привязано - уведомляем
                HAL_LOG_WARN("CLOUD", "Device NOT claimed. Waiting for claim request.");
                if (unclaimedCallback_)
                {
                    unclaimedCallback_(unclaimedCtx_);
                }
            }
        }

        void CloudStateMachine::handleAwaitingClaim()
        {
            // WiFi отключился?
            if (!wifi_->isConnected())
            {
                setState(CloudState::WifiConnecting);
                awaitingClaim_ = false;
                return;
            }

            // Если deviceId уже есть - переходим к Ready
            if (identity_.hasDeviceId())
            {
                awaitingClaim_ = false;
                setState(CloudState::Ready);
                return;
            }

            // Таймаут для polling
            const uint32_t now = HAL_MILLIS();
            if (now - lastClaimPoll_ < config_.claimPollIntervalMs)
            {
                return;
            }
            lastClaimPoll_ = now;

            // Проверяем статус привязки
            ClaimCheckResult result = api_->checkClaim(identity_.token);

            if (!result.success || !result.claimed)
            {
                // Ещё не привязано - продолжаем polling
                return;
            }

            // Привязано! Сохраняем deviceId
            identity_.setDeviceId(result.deviceId);
            store_->save(identity_);
            awaitingClaim_ = false;

            HAL_LOG_INFO("CLOUD", "Device claimed! deviceId=%s", identity_.deviceId);

            // Вызываем callback
            if (claimCompleteCallback_)
            {
                claimCompleteCallback_(identity_.deviceId, claimCompleteCtx_);
            }

            setState(CloudState::Ready);
        }

        void CloudStateMachine::handleReady()
        {
            // WiFi отключился?
            if (!wifi_->isConnected())
            {
                setState(CloudState::WifiConnecting);
                return;
            }

            // Проверяем что есть всё необходимое для MQTT
            if (!identity_.hasToken() || !identity_.hasDeviceId())
            {
                // Чего-то не хватает - возвращаемся к provisioning
                setState(CloudState::Provisioning);
                return;
            }

            // Готовы к MQTT
            setState(CloudState::MqttConnecting);
        }

        void CloudStateMachine::handleMqttConnecting()
        {
            // WiFi отключился?
            if (!wifi_->isConnected())
            {
                setState(CloudState::WifiConnecting);
                return;
            }

            // MQTT уже подключен?
            if (mqtt_->isConnected())
            {
                HAL_LOG_INFO("CLOUD", "MQTT connected!");
                setState(CloudState::Online);
                return;
            }

            // Таймаут для retry
            const uint32_t now = HAL_MILLIS();
            if (now - lastMqttAttempt_ < config_.mqttRetryIntervalMs)
            {
                return;
            }
            lastMqttAttempt_ = now;

            HAL_LOG_INFO("CLOUD", "Connecting to MQTT...");

            // Инициализируем MQTT клиент (только один раз)
            if (!mqttInitialized_)
            {
                mqtt_->begin(identity_.serialNumber, identity_.token);
                mqttInitialized_ = true;
            }

            mqtt_->connect();
        }

        void CloudStateMachine::handleOnline()
        {
            // WiFi отключился?
            if (!wifi_->isConnected())
            {
                setState(CloudState::WifiConnecting);
                return;
            }

            // MQTT отключился?
            if (!mqtt_->isConnected())
            {
                setState(CloudState::MqttConnecting);
                return;
            }

            // Обрабатываем MQTT события
            mqtt_->loop();
        }

        void CloudStateMachine::handleWaitingForMcuSerial()
        {
            // WiFi отключился — возвращаемся, но serialNumber сохранится когда придёт
            if (!wifi_->isConnected())
            {
                setState(CloudState::WifiConnecting);
                return;
            }
            // Просто ждём setMcuSerial() от RP2040 — он переведёт нас в Provisioning
        }

        // =============================================================================
        // CLAIMING
        // =============================================================================

        bool CloudStateMachine::requestClaim()
        {
            // Уже привязано?
            if (identity_.hasDeviceId())
            {
                HAL_LOG_WARN("CLOUD", "Already claimed: %s", identity_.deviceId);
                return false;
            }

            // WiFi не подключен?
            if (!wifi_->isConnected())
            {
                HAL_LOG_ERROR("CLOUD", "WiFi not connected");
                return false;
            }

            // Нет токена? Сначала provision
            if (!identity_.hasToken())
            {
                HAL_LOG_INFO("CLOUD", "No token, doing provision first...");

                ProvisionResult provResult = api_->provision(identity_.serialNumber);
                if (!provResult.success)
                {
                    HAL_LOG_ERROR("CLOUD", "Provision failed");
                    return false;
                }

                // Backend вернул isClaimed без token — нужен re-claim через UI
                if (provResult.isClaimed && provResult.token[0] == '\0')
                {
                    HAL_LOG_WARN("CLOUD", "Serial claimed but token withheld. Delete device in app first.");
                    return false;
                }

                identity_.setToken(provResult.token);
                store_->save(identity_);

                // Если уже привязано
                if (provResult.isClaimed && provResult.deviceId[0] != '\0')
                {
                    identity_.setDeviceId(provResult.deviceId);
                    store_->save(identity_);
                    HAL_LOG_INFO("CLOUD", "Already claimed: %s", identity_.deviceId);
                    return true;
                }
            }

            // Уже в процессе claiming?
            if (awaitingClaim_)
            {
                HAL_LOG_INFO("CLOUD", "Claim already in progress, PIN=%s", pendingPin_);
                return true;
            }

            // Регистрируем для получения PIN (передаём serialNumber для recovery)
            HAL_LOG_INFO("CLOUD", "Registering device for claim...");

            RegisterResult regResult = api_->registerDevice(identity_.token, identity_.serialNumber);
            if (!regResult.success)
            {
                HAL_LOG_ERROR("CLOUD", "Register failed");
                return false;
            }

            // Recovery: сервер сообщил что устройство уже привязано
            if (regResult.alreadyClaimed && regResult.deviceId[0] != '\0')
            {
                HAL_LOG_INFO("CLOUD", "Recovery: device already claimed, deviceId=%s", regResult.deviceId);
                identity_.setDeviceId(regResult.deviceId);
                store_->save(identity_);
                setState(CloudState::Ready);
                return true;
            }

            // Сохраняем PIN
            strncpy(pendingPin_, regResult.pin, sizeof(pendingPin_) - 1);
            pendingPin_[sizeof(pendingPin_) - 1] = '\0';

            awaitingClaim_ = true;
            lastClaimPoll_ = HAL_MILLIS() - config_.claimPollIntervalMs; // Сразу начать polling

            HAL_LOG_INFO("CLOUD", "PIN: %s (expires in %us)",
                         pendingPin_, regResult.remainingSeconds);

            // Вызываем callback с PIN
            if (claimPinCallback_)
            {
                claimPinCallback_(pendingPin_, regResult.remainingSeconds, claimPinCtx_);
            }

            setState(CloudState::AwaitingClaim);
            return true;
        }

        // =============================================================================
        // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // =============================================================================

        void CloudStateMachine::setState(CloudState newState)
        {
            if (state_ == newState)
            {
                return;
            }

            CloudState oldState = state_;
            state_ = newState;

            HAL_LOG_INFO("CLOUD", "State: %s -> %s",
                         cloudStateToString(oldState),
                         cloudStateToString(newState));

            // Вызываем callback
            if (stateCallback_)
            {
                stateCallback_(oldState, newState, stateCallbackCtx_);
            }
        }

        void CloudStateMachine::setMcuSerial(const char *mcuSerial)
        {
            if (!mcuSerial || mcuSerial[0] == '\0') return;

            // Уже верифицирован и совпадает — ничего не делаем
            if (serialVerified_ && strcmp(identity_.serialNumber, mcuSerial) == 0) return;

            // MISMATCH: NVS serial != UART serial — сушилка заменена
            if (identity_.hasSerialNumber() && strcmp(identity_.serialNumber, mcuSerial) != 0)
            {
                HAL_LOG_WARN("CLOUD", "MCU serial MISMATCH: NVS=%s UART=%s — dryer changed?",
                             identity_.serialNumber, mcuSerial);
                serialVerified_ = false;

                // Отключаем MQTT для предотвращения подмены данных
                if (mqtt_->isConnected())
                {
                    mqtt_->disconnect();
                    mqttInitialized_ = false;
                }

                // НЕ сбрасываем NVS — пользователь может вернуть Link обратно.
                // Уведомляем через unclaimedCallback (RP2040 покажет ошибку на экране).
                if (unclaimedCallback_)
                {
                    unclaimedCallback_(unclaimedCtx_);
                }
                return;
            }

            // MATCH (или первый boot) — верификация пройдена
            identity_.setSerialNumber(mcuSerial);
            serialVerified_ = true;
            store_->save(identity_);

            HAL_LOG_INFO("CLOUD", "mcuSerial verified: %s", mcuSerial);

            // Если ждали serial от RP2040 — теперь можно идти в Provisioning
            if (state_ == CloudState::WaitingForMcuSerial)
            {
                lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs; // сразу
                setState(CloudState::Provisioning);
            }
        }

        void CloudStateMachine::setStateChangeCallback(CloudStateChangeCallback cb, void *ctx)
        {
            stateCallback_ = cb;
            stateCallbackCtx_ = ctx;
        }

        void CloudStateMachine::setClaimPinCallback(ClaimPinCallback cb, void *ctx)
        {
            claimPinCallback_ = cb;
            claimPinCtx_ = ctx;
        }

        void CloudStateMachine::setClaimCompleteCallback(ClaimCompleteCallback cb, void *ctx)
        {
            claimCompleteCallback_ = cb;
            claimCompleteCtx_ = ctx;
        }

        void CloudStateMachine::setUnclaimedCallback(UnclaimedCallback cb, void *ctx)
        {
            unclaimedCallback_ = cb;
            unclaimedCtx_ = ctx;
        }

        bool CloudStateMachine::refreshToken()
        {
            if (!wifi_->isConnected())
            {
                HAL_LOG_WARN("CLOUD", "refreshToken: no WiFi");
                return false;
            }
            if (!identity_.hasSerialNumber())
            {
                HAL_LOG_WARN("CLOUD", "refreshToken: no serial");
                return false;
            }

            // Cooldown 30 секунд между попытками
            const uint32_t now = HAL_MILLIS();
            if (lastTokenRefreshMs_ != 0 && now - lastTokenRefreshMs_ < 30000u)
            {
                HAL_LOG_INFO("CLOUD", "refreshToken: cooldown active");
                return false;
            }
            lastTokenRefreshMs_ = now;

            HAL_LOG_INFO("CLOUD", "refreshToken: calling provision for serial=%s", identity_.serialNumber);
            ProvisionResult result = api_->provision(identity_.serialNumber);

            if (!result.success || result.token[0] == '\0')
            {
                HAL_LOG_WARN("CLOUD", "refreshToken: provision failed");
                return false;
            }

            identity_.setToken(result.token);
            store_->save(identity_);
            HAL_LOG_INFO("CLOUD", "refreshToken: token updated in NVS");
            return true;
        }

    } // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
