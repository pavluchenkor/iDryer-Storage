/**
 * @file ha_mqtt_client.h
 * @brief MQTT клиент для Home Assistant (локальный брокер)
 *
 * Автоматически ищет HA через mDNS (homeassistant.local)
 * и подключается к локальному Mosquitto брокеру.
 */

#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <functional>

namespace idryer {
namespace ha {

/// Максимальные размеры буферов для HA MQTT
#define HA_MQTT_BUFFER_SIZE 2048
#define HA_TOPIC_BUFFER_SIZE 128
#define HA_MQTT_KEEPALIVE 60

/// Результат mDNS поиска
struct HaDiscoveryResult {
    bool found = false;
    IPAddress ip;
    uint16_t port = 1883;  // Стандартный порт Mosquitto
};

/**
 * @brief MQTT клиент для Home Assistant
 *
 * Особенности:
 * - Автопоиск через mDNS
 * - Простая авторизация (username/password опционально)
 * - Публикация в простые топики
 * - Без TLS (локальная сеть)
 */
class HaMqttClient {
public:
    HaMqttClient();

    /**
     * @brief Поиск Home Assistant в локальной сети
     * @param host Если задан — используется напрямую (без mDNS). Дефолт: homeassistant.local
     * @return true если HA найден
     */
    bool discover(const char* host = nullptr);

    /**
     * @brief Ручная настройка адреса брокера (альтернатива discover)
     * @param ip IP адрес MQTT брокера
     * @param port Порт (по умолчанию 1883)
     */
    void setServer(IPAddress ip, uint16_t port = 1883);

    /**
     * @brief Подключение к MQTT брокеру
     * @param clientId Уникальный ID клиента (обычно serialNumber)
     * @param username Опционально, для авторизации
     * @param password Опционально, для авторизации
     * @return true если подключение успешно
     */
    bool connect(const char* clientId,
                 const char* username = nullptr,
                 const char* password = nullptr);

    /**
     * @brief Проверка подключения
     */
    bool isConnected();

    /**
     * @brief Проверка что HA был найден через discover()
     */
    bool isDiscovered() const { return discovered_; }

    /**
     * @brief Обработка MQTT loop (вызывать в главном цикле)
     */
    void loop();

    /**
     * @brief Публикация в топик
     * @param topic Топик (полный путь, например "homeassistant/sensor/idryer_U1_temp/state")
     * @param payload Данные для публикации
     * @param retained Retained флаг (по умолчанию false)
     * @return true если публикация успешна
     */
    bool publish(const char* topic, const char* payload, bool retained = false);

    /**
     * @brief Подписка на топик
     */
    bool subscribe(const char* topic);

    /**
     * @brief Callback для входящих сообщений
     */
    using MessageCallback = std::function<void(const char* topic, const char* payload)>;
    void setMessageCallback(MessageCallback cb) { messageCallback_ = cb; }

    /**
     * @brief Получить результат последнего поиска
     */
    const HaDiscoveryResult& getDiscoveryResult() const { return discoveryResult_; }

private:
    WiFiClient wifiClient_;
    PubSubClient mqttClient_;

    HaDiscoveryResult discoveryResult_;
    bool discovered_ = false;
    bool initialized_ = false;

    char clientId_[32];
    char username_[64];
    char password_[64];

    MessageCallback messageCallback_;

    /// Буфер для формирования топиков
    char topicBuffer_[HA_TOPIC_BUFFER_SIZE];
};

} // namespace ha
} // namespace idryer

#endif // ESP32
