/**
 * @file ArduinoCredentialStore.cpp
 * @brief Реализация хранилища credentials для Arduino Framework (ESP32)
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoCredentialStore.h"
#include "../../hal/hal_types.h"
#include <string.h>

namespace idryer {

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ
// =============================================================================

bool ArduinoCredentialStore::begin()
{
    // NVS всегда доступна на ESP32 после инициализации Arduino
    return true;
}

// =============================================================================
// ЗАГРУЗКА
// =============================================================================

bool ArduinoCredentialStore::load(DeviceIdentity& identity)
{
    // Очищаем структуру
    identity.clear();

    // Открываем namespace на чтение
    if (!prefs_.begin(kNamespace, true)) {
        HAL_LOG_ERROR("STORE", "Failed to open NVS for reading");
        return false;
    }

    // Читаем token
    String token = prefs_.getString("token", "");
    if (token.length() > 0) {
        identity.setToken(token.c_str());
    }

    // Читаем deviceId
    String deviceId = prefs_.getString("deviceId", "");
    if (deviceId.length() > 0) {
        identity.setDeviceId(deviceId.c_str());
    }

    // Читаем serialNumber
    String serial = prefs_.getString("serial", "");
    if (serial.length() > 0) {
        identity.setSerialNumber(serial.c_str());
    }

    prefs_.end();

    HAL_LOG_DEBUG("STORE", "Loaded: serial=%s token=%s deviceId=%s",
                  identity.hasSerialNumber() ? identity.serialNumber : "(none)",
                  identity.hasToken() ? "yes" : "no",
                  identity.hasDeviceId() ? identity.deviceId : "(none)");

    // Считаем загрузку успешной, если хотя бы токен есть
    return identity.hasToken();
}

// =============================================================================
// СОХРАНЕНИЕ
// =============================================================================

bool ArduinoCredentialStore::save(const DeviceIdentity& identity)
{
    // Открываем namespace на запись
    if (!prefs_.begin(kNamespace, false)) {
        HAL_LOG_ERROR("STORE", "Failed to open NVS for writing");
        return false;
    }

    // Сохраняем все поля
    prefs_.putString("token", identity.token);
    prefs_.putString("deviceId", identity.deviceId);
    prefs_.putString("serial", identity.serialNumber);

    prefs_.end();

    HAL_LOG_INFO("STORE", "Saved credentials to NVS");
    return true;
}

// =============================================================================
// ОЧИСТКА
// =============================================================================

void ArduinoCredentialStore::clear()
{
    if (!prefs_.begin(kNamespace, false)) {
        HAL_LOG_ERROR("STORE", "Failed to open NVS for clearing");
        return;
    }

    prefs_.clear();
    prefs_.end();

    HAL_LOG_INFO("STORE", "Cleared all credentials from NVS");
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
