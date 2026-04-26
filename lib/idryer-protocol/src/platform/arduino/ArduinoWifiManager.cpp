/**
 * @file ArduinoWifiManager.cpp
 * @brief Реализация WiFi менеджера для Arduino Framework (ESP32)
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoWifiManager.h"
#include "../../hal/hal_types.h"
#include <esp_wifi.h>
#include <string.h>

namespace idryer {

// =============================================================================
// КОНСТРУКТОР
// =============================================================================

ArduinoWifiManager::ArduinoWifiManager()
{
    memset(ssid_, 0, sizeof(ssid_));
    memset(password_, 0, sizeof(password_));
}

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ
// =============================================================================

void ArduinoWifiManager::begin(const char* ssid, const char* password)
{
    // Сохраняем credentials в буферы
    if (ssid) {
        strncpy(ssid_, ssid, sizeof(ssid_) - 1);
        ssid_[sizeof(ssid_) - 1] = '\0';
    }
    if (password) {
        strncpy(password_, password, sizeof(password_) - 1);
        password_[sizeof(password_) - 1] = '\0';
    }

    // Настройка WiFi модуля
    WiFi.persistent(false);           // Не сохраняем в NVS (мы сами управляем)
    WiFi.mode(WIFI_STA);              // Station mode (клиент)
    WiFi.setAutoReconnect(true);      // Автопереподключение

    // Отключаем WiFi power save для стабильного соединения
    esp_wifi_set_ps(WIFI_PS_NONE);

    HAL_LOG_INFO("WIFI", "Initialized with SSID: %s", ssid_);
}

// =============================================================================
// ПОДКЛЮЧЕНИЕ
// =============================================================================

bool ArduinoWifiManager::connect()
{
    wl_status_t status = WiFi.status();

    // Уже подключены?
    if (status == WL_CONNECTED) {
        return true;
    }

    // Первое подключение - сканируем сети для диагностики
    if (!scanLogged_) {
        HAL_LOG_INFO("WIFI", "Scanning networks...");

        const int networkCount = WiFi.scanNetworks(false, true);
        if (networkCount > 0) {
            for (int i = 0; i < networkCount; ++i) {
                HAL_LOG_DEBUG("WIFI", "AP %d: %s (RSSI=%d dBm)",
                             i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            }
        }
        WiFi.scanDelete();
        scanLogged_ = true;

        HAL_LOG_INFO("WIFI", "Connecting to SSID: %s", ssid_);
        WiFi.begin(ssid_, password_);
    } else {
        // Повторное подключение после потери связи
        HAL_LOG_DEBUG("WIFI", "Status: %d, reconnecting...", status);
        WiFi.begin(ssid_, password_);
    }

    return false; // Асинхронное подключение - результат позже
}

// =============================================================================
// СТАТУС
// =============================================================================

bool ArduinoWifiManager::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

void ArduinoWifiManager::disconnect()
{
    WiFi.disconnect();
    scanLogged_ = false;
}

// =============================================================================
// ИНФОРМАЦИЯ О СЕТИ
// =============================================================================

void ArduinoWifiManager::getLocalIP(char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0) return;

    // Получаем IP и конвертируем в строку
    String ip = WiFi.localIP().toString();
    strncpy(buffer, ip.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}

void ArduinoWifiManager::getSSID(char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0) return;

    if (isConnected()) {
        // Получаем SSID подключенной сети
        String ssid = WiFi.SSID();
        strncpy(buffer, ssid.c_str(), bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    } else {
        // Не подключены - возвращаем пустую строку
        buffer[0] = '\0';
    }
}

int ArduinoWifiManager::getRSSI()
{
    return WiFi.RSSI();
}

void ArduinoWifiManager::getMacAddress(char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0) return;

    // Получаем MAC и конвертируем в строку
    String mac = WiFi.macAddress();
    strncpy(buffer, mac.c_str(), bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}

// =============================================================================
// LOOP
// =============================================================================

void ArduinoWifiManager::loop()
{
    // Arduino WiFi работает асинхронно через события
    // Дополнительная обработка не требуется
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
