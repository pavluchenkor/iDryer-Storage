/**
 * @file IWifiManager.h
 * @brief Платформенно-независимый интерфейс Wi-Fi.
 */

#pragma once

#include "../../core/config.h"
#include <stdint.h>

namespace idryer {

/// Реализация должна быть неблокирующей в `connect/loop`.
class IWifiManager {
public:
    virtual ~IWifiManager() = default;

    /// Сохраняет SSID/пароль для последующего `connect()`.
    virtual void begin(const char* ssid, const char* password) = 0;

    /// Запускает/поддерживает подключение, не должен блокировать loop надолго.
    virtual bool connect() = 0;

    virtual bool isConnected() = 0;

    virtual void disconnect() = 0;

    /// `bufferSize >= IDRYER_MAX_IP_LEN`, формат `A.B.C.D`.
    virtual void getLocalIP(char* buffer, size_t bufferSize) = 0;

    /// `bufferSize >= IDRYER_MAX_SSID_LEN`.
    virtual void getSSID(char* buffer, size_t bufferSize) = 0;

    virtual int getRSSI() = 0;

    /// `bufferSize >= IDRYER_MAX_MAC_LEN`, формат `AA:BB:CC:DD:EE:FF`.
    virtual void getMacAddress(char* buffer, size_t bufferSize) = 0;

    /// Вызывается каждый цикл для обслуживания стека реализации.
    virtual void loop() = 0;
};

} // namespace idryer
