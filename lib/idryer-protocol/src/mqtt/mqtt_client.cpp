/**
 * @file mqtt_client.cpp
 * @brief MQTT Client Implementation
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "mqtt_client.h"
#include "root_ca.h"
#include <esp_system.h>
#include <time.h>
#include <string.h>

#ifdef ARDUINO
#include "secrets.h"
#endif

// ANSI цветовые коды для терминала
// Формат: \033[XXm где XX - код цвета
#define ANSI_RESET "\033[0m" // Сброс форматирования

// Обычные цвета (30-37)
#define ANSI_BLACK "\033[30m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN "\033[36m"
#define ANSI_WHITE "\033[37m"

// Яркие цвета (90-97)
#define ANSI_BRIGHT_BLACK "\033[90m" // Серый
#define ANSI_BRIGHT_RED "\033[91m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_YELLOW "\033[93m"
#define ANSI_BRIGHT_BLUE "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN "\033[96m"
#define ANSI_BRIGHT_WHITE "\033[97m"

// Стили текста
#define ANSI_BOLD "\033[1m"      // Жирный
#define ANSI_DIM "\033[2m"       // Тусклый
#define ANSI_UNDERLINE "\033[4m" // Подчеркнутый
#define ANSI_BLINK "\033[5m"     // Мигающий
#define ANSI_REVERSE "\033[7m"   // Инверсия цветов

// Singleton instance для callback
MqttClient *MqttClient::instance_ = nullptr;

void MqttClient::begin(const char *serialNumber, const char *token)
{
    // Очищаем буферы
    memset(serialNumber_, 0, sizeof(serialNumber_));
    memset(token_, 0, sizeof(token_));
    memset(clientId_, 0, sizeof(clientId_));

    // Сохраняем учетные данные
    if (serialNumber && serialNumber[0])
    {
        strncpy(serialNumber_, serialNumber, sizeof(serialNumber_) - 1);
        strncpy(clientId_, serialNumber, sizeof(clientId_) - 1);
    }
    if (token && token[0])
    {
        strncpy(token_, token, sizeof(token_) - 1);
    }

    // Настраиваем TLS (только для WiFiClientSecure)
#if MQTT_USE_TLS
    wifiClient_.setCACert(ROOT_CA_LETSENCRYPT);
    wifiClient_.setTimeout(10);
#endif

    // Настраиваем MQTT клиент
    mqttClient_.setClient(wifiClient_);
    mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient_.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient_.setKeepAlive(IDRYER_MQTT_KEEPALIVE);
    mqttClient_.setCallback(MqttClient::mqttCallback);

    // Singleton для callback
    instance_ = this;

    // Устанавливаем флаг инициализации
    initialized_ = true;

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Init: broker=%s:%d serial=%s\n",
                        MQTT_BROKER, MQTT_PORT, serialNumber_);
#endif
}

void MqttClient::setCommandCallback(CommandCallback callback)
{
    commandCallback_ = callback;
}

void MqttClient::disconnect()
{
    if (mqttClient_.connected())
    {
        mqttClient_.disconnect();
    }
    initialized_ = false;
}

bool MqttClient::connect()
{
    // Проверка инициализации
    if (!initialized_)
    {
        return false;
    }

    if (mqttClient_.connected())
    {
        return true;
    }

    // Проверка что credentials заданы
    if (!clientId_[0] || !serialNumber_[0] || !token_[0])
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf("[MQTT] ERROR: Empty credentials! clientId='%s' serial='%s' token='%s'\n",
                            clientId_[0] ? clientId_ : "(empty)",
                            serialNumber_[0] ? serialNumber_ : "(empty)",
                            token_[0] ? "***" : "(empty)");
#endif
        return false;
    }

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Connecting as %s...\n", clientId_);
#endif

    // Формируем LWT топик: idryer/{serial}/offline
    char lwtTopic[128];
    idryer_make_topic(lwtTopic, sizeof(lwtTopic), serialNumber_, IDRYER_TOPIC_OFFLINE);

    // =========================================================================
    //            ██╗    ██╗ █████╗ ██████╗ ███╗   ██╗██╗███╗   ██╗ ██████╗
    //            ██║    ██║██╔══██╗██╔══██╗████╗  ██║██║████╗  ██║██╔════╝
    //            ██║ █╗ ██║███████║██████╔╝██╔██╗ ██║██║██╔██╗ ██║██║  ███╗
    //            ██║███╗██║██╔══██║██╔══██╗██║╚██╗██║██║██║╚██╗██║██║   ██║
    //            ╚███╔███╔╝██║  ██║██║  ██║██║ ╚████║██║██║ ╚████║╚██████╔╝
    //             ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═══╝ ╚═════╝
    //
    //  НЕ ТРОГАТЬ `kMqttCleanSession`. ДОЛЖЕН ОСТАВАТЬСЯ `false`.
    //  DO NOT CHANGE `kMqttCleanSession`. MUST STAY `false`.
    //
    //  Persistent MQTT-сессия (clean_session = 0) на стороне клиента —
    //  обязательное условие работы команд с портала. Если поставить `true`:
    //
    //    • Брокер при каждом CONNECT будет стирать подписку
    //      `idryer/<serial>/commands/#` на стороне Mosquitto.
    //    • На устройстве тогда есть ровно один шанс успешно отправить
    //      SUBSCRIBE после CONNECT. Если SUBSCRIBE не долетел (WiFi лаг,
    //      keepalive-таймаут, half-closed TCP, переполнение буфера
    //      PubSubClient) — подписки нет, команды от портала просто не
    //      доставляются. Устройство работает, publish идёт, reconnect
    //      идёт, а drying/stop/find/и т.п. в лог не попадают.
    //    • Диагностика в Mosquitto log: `c1` в строке `New client connected`
    //      и отсутствие `New subscription from <serial>`.
    //
    //  Это уже проверено на живом стенде (2026-04-20): с clean_session=1
    //  устройства молча теряли подписку после бурста команд от портала,
    //  с clean_session=0 — команды стали стабильно доходить.
    //
    //  Признак рабочей прошивки в Mosquitto:
    //    `New client connected ... as <serial> (p2, c0, k60, ...)`
    //                                          ^^^ c0 = clean_session=false
    // =========================================================================
    static constexpr bool kMqttCleanSession = false;

    // Подключаемся с LWT + persistent session.
    bool connected = mqttClient_.connect(
        clientId_,            // Client ID
        serialNumber_,        // Username
        token_,               // Password (deviceToken)
        lwtTopic,             // LWT topic
        1,                    // LWT QoS 1
        false,                // LWT retain: false
        "{}",                 // LWT payload
        kMqttCleanSession     // см. предупреждение выше — должен быть `false`
    );

    if (!connected)
    {
#ifdef DEBUG_SERIAL
        int state = mqttClient_.state();
        DEBUG_SERIAL.printf("[MQTT] Connection failed: %d\n", state);
#endif
        return false;
    }

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println("[MQTT] Connected!");
#endif

    // Подписываемся на команды. Проверяем возвращаемое значение —
    // PubSubClient::subscribe возвращает false если пакет не удалось
    // записать в сокет (не connected, переполнение буфера и т.п.).
    // Раньше мы игнорировали результат и безусловно печатали "Subscribed",
    // из-за чего на брокере подписки не было, а в логе устройства — была.
    const char *cmdTopic = makeTopic(IDRYER_TOPIC_CMD_WILDCARD);

    bool subscribeOk = false;
    for (uint8_t attempt = 0; attempt < 3 && !subscribeOk; ++attempt) {
        subscribeOk = mqttClient_.subscribe(cmdTopic, IDRYER_QOS_COMMANDS);
        if (!subscribeOk) {
#ifdef DEBUG_SERIAL
            DEBUG_SERIAL.printf("[MQTT] SUBSCRIBE failed (attempt %u): %s\n",
                                attempt + 1, cmdTopic);
#endif
            delay(50);  // короткая пауза перед повтором
        }
    }

    if (!subscribeOk) {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf("[MQTT] SUBSCRIBE could not be sent after retries. "
                            "Disconnecting to force reconnect.\n");
#endif
        // Если SUBSCRIBE не ушёл — сессия бесполезна (команды не придут).
        // Форсируем reconnect: loop() заметит и соберёт заново.
        mqttClient_.disconnect();
        return false;
    }

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Subscribed: %s (QoS %d) OK\n",
                        cmdTopic, IDRYER_QOS_COMMANDS);
#endif

    return true;
}

bool MqttClient::isConnected()
{
    return mqttClient_.connected();
}

void MqttClient::loop()
{
    // Не пытаемся подключаться пока не инициализирован
    if (!initialized_)
    {
        return;
    }

    if (!mqttClient_.connected())
    {
        // Auto-reconnect
        connect();
    }
    mqttClient_.loop();
}

// ============================================================================
// Публикация топиков (используя константы из idryer_topics.h)
// ============================================================================

// Маппинг DryerUart::DeviceType → строка для MQTT JSON.
// Unknown (0) возвращает nullptr — поле в info JSON опускается (legacy на стороне портала).
static const char *deviceTypeToString(uint8_t deviceType)
{
    switch (deviceType)
    {
    case 0x01: return "dryer";
    case 0x02: return "heater";
    case 0x03: return "telemetry";
    case 0x04: return "link";
    case 0x05: return "iheater_link";
    default:   return nullptr; // Unknown или будущие значения → не публикуем поле
    }
}

bool MqttClient::publishInfo(const char *hwVersion, const char *fwVersion, uint32_t workTimeCounter,
                             uint8_t unitsCount, const DryerUart::UnitConfig *units,
                             const char *mcuSerial, uint8_t deviceType)
{
    DynamicJsonDocument doc(1024);

    // Формируем JSON согласно API (см. idryer_topics.h)
    doc["hardwareVersion"] = hwVersion;
    doc["firmwareVersion"] = fwVersion;
    doc["workTimeCounter"] = workTimeCounter;
    doc["unitsCount"] = unitsCount;
    if (mcuSerial && mcuSerial[0] != '\0') {
        doc["mcuSerial"] = mcuSerial;
    }
    if (const char *dtStr = deviceTypeToString(deviceType)) {
        doc["deviceType"] = dtStr;
    }

    // Добавляем конфигурацию units
    if (units && unitsCount > 0)
    {
        JsonArray unitsArray = doc.createNestedArray("units");
        for (uint8_t i = 0; i < unitsCount && i < 4; ++i)
        {
            const auto &unit = units[i];
            JsonObject unitObj = unitsArray.createNestedObject();
            unitObj["unitId"] = unit.unitId;

            // Hardware capabilities (битовые флаги → объект с boolean полями)
            JsonObject caps = unitObj.createNestedObject("capabilities");
            caps["heater"] = (unit.capabilities & DryerUart::UnitCapabilities::HEATER) != 0;
            caps["fan"] = (unit.capabilities & DryerUart::UnitCapabilities::FAN) != 0;
            caps["servo"] = (unit.capabilities & DryerUart::UnitCapabilities::SERVO) != 0;
            caps["RhAirSensor"] = (unit.capabilities & DryerUart::UnitCapabilities::RH_AIR_SENSOR) != 0;
            caps["TempAirSensor"] = (unit.capabilities & DryerUart::UnitCapabilities::TEMP_AIR_SENSOR) != 0;
            caps["TempHeaterSensor"] = (unit.capabilities & DryerUart::UnitCapabilities::TEMP_HEATER_SENSOR) != 0;

            // Массив scales - добавляем все валидные (не 0xFF)
            JsonArray scalesArray = unitObj.createNestedArray("scales");
            for (uint8_t j = 0; j < 4; ++j)
            {
                if (unit.scales[j] != 0xFF)
                {
                    scalesArray.add(unit.scales[j]);
                }
            }

            // Массив rfid - добавляем все валидные (не 0xFF)
            JsonArray rfidArray = unitObj.createNestedArray("rfid");
            for (uint8_t j = 0; j < 4; ++j)
            {
                if (unit.rfid[j] != 0xFF)
                {
                    rfidArray.add(unit.rfid[j]);
                }
            }
        }
    }

    char timestamp[32];
    doc["timestamp"] = getIsoTimestamp(timestamp);

    // Используем константы: IDRYER_TOPIC_INFO, IDRYER_RETAINED_INFO, IDRYER_QOS_INFO
    return publishJson(IDRYER_TOPIC_INFO, doc, IDRYER_RETAINED_INFO, IDRYER_QOS_INFO);
}

bool MqttClient::publishTelemetry(JsonDocument &json)
{
    // Используем константы: IDRYER_TOPIC_TELEMETRY, IDRYER_RETAINED_TELEMETRY, IDRYER_QOS_TELEMETRY
    return publishJson(IDRYER_TOPIC_TELEMETRY, json, IDRYER_RETAINED_TELEMETRY, IDRYER_QOS_TELEMETRY);
}

bool MqttClient::publishStatus(JsonDocument &json)
{
    // Используем константы: IDRYER_TOPIC_STATUS, IDRYER_RETAINED_STATUS, IDRYER_QOS_STATUS
    return publishJson(IDRYER_TOPIC_STATUS, json, IDRYER_RETAINED_STATUS, IDRYER_QOS_STATUS);
}

bool MqttClient::publishWeights(JsonDocument &json)
{
    // Используем константы: IDRYER_TOPIC_WEIGHTS, IDRYER_RETAINED_WEIGHTS, IDRYER_QOS_WEIGHTS
    return publishJson(IDRYER_TOPIC_WEIGHTS, json, IDRYER_RETAINED_WEIGHTS, IDRYER_QOS_WEIGHTS);
}

bool MqttClient::publishRfid(JsonDocument &json)
{
    // Используем константы: IDRYER_TOPIC_RFID, IDRYER_RETAINED_RFID, IDRYER_QOS_RFID
    return publishJson(IDRYER_TOPIC_RFID, json, IDRYER_RETAINED_RFID, IDRYER_QOS_RFID);
}

bool MqttClient::publishEvent(JsonDocument &json)
{
    // Используем константы: IDRYER_TOPIC_EVENTS, IDRYER_RETAINED_EVENTS, IDRYER_QOS_EVENTS
    return publishJson(IDRYER_TOPIC_EVENTS, json, IDRYER_RETAINED_EVENTS, IDRYER_QOS_EVENTS);
}

bool MqttClient::publishIntegrationsStatus(JsonDocument &json)
{
    // integrations/status: retained + QoS 1 — новый подписчик сразу видит состояние.
    return publishJson(IDRYER_TOPIC_INTEGRATIONS_STATUS, json,
                       /*retained=*/true, IDRYER_QOS_INTEGRATIONS_STATUS);
}

bool MqttClient::publishConfig(JsonDocument &json)
{
    // Используем константы: IDRYER_TOPIC_CONFIG, IDRYER_RETAINED_CONFIG, IDRYER_QOS_CONFIG
    return publishJson(IDRYER_TOPIC_CONFIG, json, IDRYER_RETAINED_CONFIG, IDRYER_QOS_CONFIG);
}

uint16_t MqttClient::publishConfigRaw(const char *json, size_t length)
{
    if (!mqttClient_.connected() || !json || length == 0)
    {
        return 0;
    }

    const char *topic = makeTopic(IDRYER_TOPIC_CONFIG);

    // Если влезает в один пакет — отправляем целиком
    if (length <= MQTT_CONFIG_CHUNK_SIZE)
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT] → Config (full): %u bytes" ANSI_RESET "\n", length);
#endif
        bool success = mqttClient_.publish(topic, json, IDRYER_RETAINED_CONFIG);
        return success ? 1 : 0;
    }

    // Фрагментируем
    uint16_t tid = ++configTransferId_;
    size_t offset = 0;
    uint16_t idx = 0;
    uint16_t sentCount = 0;

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT] → Config (chunked): %u bytes, tid=%u" ANSI_RESET "\n", length, tid);
#endif

    // Буфер для чанка (обёртка + данные)
    // Формат: {"tid":N,"idx":N,"total":N,"last":bool,"d":"..."}
    // Overhead: ~50 байт для обёртки
    char *chunkBuf = (char *)malloc(MQTT_CONFIG_CHUNK_SIZE + 100);
    if (!chunkBuf)
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.println("[MQTT] Out of memory for chunk buffer!");
#endif
        return 0;
    }

    while (offset < length)
    {
        size_t chunkDataLen = (length - offset > MQTT_CONFIG_CHUNK_SIZE)
                                  ? MQTT_CONFIG_CHUNK_SIZE
                                  : (length - offset);
        bool isLast = (offset + chunkDataLen >= length);

        // Формируем JSON чанка
        // Экранируем данные для JSON строки
        // Для простоты используем ArduinoJson
        DynamicJsonDocument chunkDoc(MQTT_CONFIG_CHUNK_SIZE + 200);
        chunkDoc["tid"] = tid;
        chunkDoc["idx"] = idx;
        chunkDoc["total"] = length;
        chunkDoc["last"] = isLast;

        // Копируем данные как строку
        // Важно: данные уже являются частью JSON, экранирование не нужно
        // если мы передаём их как часть JSON строки
        char *dataBuf = (char *)malloc(chunkDataLen + 1);
        if (!dataBuf)
        {
            free(chunkBuf);
            return 0;
        }
        memcpy(dataBuf, json + offset, chunkDataLen);
        dataBuf[chunkDataLen] = '\0';
        chunkDoc["d"] = dataBuf;

        size_t written = serializeJson(chunkDoc, chunkBuf, MQTT_CONFIG_CHUNK_SIZE + 100);
        free(dataBuf);

#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT]   chunk[%u]: %u bytes data, %u bytes total, last=%d" ANSI_RESET "\n",
                            idx, chunkDataLen, written, isLast);
#endif

        bool success = mqttClient_.publish(topic, chunkBuf, IDRYER_RETAINED_CONFIG);
        if (!success)
        {
#ifdef DEBUG_SERIAL
            DEBUG_SERIAL.printf("[MQTT] Failed to publish chunk %u!\n", idx);
#endif
            free(chunkBuf);
            return 0;
        }

        offset += chunkDataLen;
        idx++;
        sentCount++;

        // Даём время на отправку
        delay(10);
    }

    free(chunkBuf);

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT] → Config complete: %u chunks sent" ANSI_RESET "\n", sentCount);
#endif

    return sentCount;
}

bool MqttClient::publishConfigDelta(const char *json, size_t length)
{
    if (!mqttClient_.connected() || !json || length == 0)
    {
        return false;
    }

    const char *topic = makeTopic(IDRYER_TOPIC_CONFIG_DELTA);

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT] → Config delta: %u bytes" ANSI_RESET "\n", length);
#endif

    return mqttClient_.publish(topic, json, IDRYER_RETAINED_CONFIG_DELTA);
}

// ============================================================================
// Обработка команд
// ============================================================================

void MqttClient::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    if (instance_)
    {
        char *payloadStr = (char *)malloc(length + 1);
        if (payloadStr)
        {
            memcpy(payloadStr, payload, length);
            payloadStr[length] = '\0';
            instance_->handleMessage(topic, payloadStr, length);
            free(payloadStr);
        }
    }
}

void MqttClient::handleMessage(const char *topic, const char *payload, size_t length)
{
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("\n" ANSI_BLUE "[MQTT] ← Message received" ANSI_RESET "\n");
    DEBUG_SERIAL.printf(ANSI_BRIGHT_CYAN "[MQTT] Topic: %s" ANSI_RESET "\n", topic);
    DEBUG_SERIAL.printf("[MQTT] Payload (%u bytes): ", length);
    DEBUG_SERIAL.write((const uint8_t *)payload, length);
    DEBUG_SERIAL.printf("\n");
#endif

    // Парсим топик: idryer/{serial}/commands/{command}
    // Используем константы для проверки префикса команд
    const char *cmdPrefix = "/commands/";
    const char *cmdStart = strstr(topic, cmdPrefix);
    if (!cmdStart)
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf("[MQTT] Not a command topic, ignoring\n");
#endif
        return;
    }
    cmdStart += strlen(cmdPrefix); // Пропускаем "/commands/"

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Command: %s\n", cmdStart);
#endif

    // Парсим JSON payload команды
    // Формат команд см. в idryer_topics.h (IDRYER_TOPIC_CMD_*)
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err)
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf("[MQTT] ❌ JSON parse error: %s\n", err.c_str());
#endif
        return;
    }

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] ✅ JSON parsed successfully\n");
#endif

    // Вызываем callback для обработки команды
    if (commandCallback_)
    {
        commandCallback_(cmdStart, doc.as<JsonObjectConst>());
    }
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

const char *MqttClient::makeTopic(const char *suffix)
{
    // Используем функцию из idryer_topics.h
    idryer_make_topic(topicBuffer_, sizeof(topicBuffer_), serialNumber_, suffix);
    return topicBuffer_;
}

bool MqttClient::publishJson(const char *suffix, JsonDocument &json, bool retained, int qos)
{
    if (!mqttClient_.connected())
    {
        return false;
    }

    // Добавляем timestamp если его нет
    // Все топики ДОЛЖНЫ содержать timestamp в формате ISO 8601 (UTC)
    if (!json.containsKey("timestamp"))
    {
        char timestamp[32];
        json["timestamp"] = getIsoTimestamp(timestamp);
    }

    // Сериализуем JSON в строку
    size_t jsonSize = measureJson(json);
    char *jsonBuffer = (char *)malloc(jsonSize + 1);
    if (!jsonBuffer)
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.println("[MQTT] Out of memory!");
#endif
        return false;
    }

    size_t written = serializeJson(json, jsonBuffer, jsonSize + 1);

    // Формируем полный топик используя константы из idryer_topics.h
    const char *topic = makeTopic(suffix);

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT] → Publish: %s (len=%u qos=%d retained=%d)" ANSI_RESET "\n",
                        topic, written, qos, retained);
    DEBUG_SERIAL.printf(ANSI_CYAN "[MQTT] Payload: %s" ANSI_RESET "\n", jsonBuffer);
#endif

    // Публикуем в MQTT брокер
    // retained = true означает что MQTT брокер сохранит последнее сообщение
    // и отправит его новым подписчикам
    bool success = mqttClient_.publish(topic, jsonBuffer, retained);
    free(jsonBuffer);

    if (!success)
    {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.println("[MQTT] Publish failed!");
#endif
    }

    return success;
}

// ============================================================================
// Утилиты: Timestamp и UUID
// ============================================================================

char *MqttClient::getIsoTimestamp(char *buffer)
{
    // Получаем текущее время
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // Форматируем в ISO 8601 (UTC)
    strftime(buffer, 32, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return buffer;
}

char *MqttClient::generateUuid(char *buffer)
{
    // UUID v4 формат: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // где x = random hex, y = [8,9,a,b]
    //
    // Используется для sessionId согласно API (см. idryer_topics.h, IDRYER_TOPIC_STATUS)
    // - Генерируется устройством при старте DRYING/STORAGE/PROFILE
    // - Передается в status топике
    // - Backend использует для создания/обновления dryingSessionNew
    //
    // Пример: "a7b3c9d1-e4f5-6789-0abc-def123456789"

    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();

    // Формируем UUID v4 (RFC 4122)
    snprintf(buffer, 37,
             "%08x-%04x-4%03x-%04x-%012llx",
             r1,
             (r2 >> 16) & 0xFFFF,
             r2 & 0x0FFF,
             ((r3 >> 16) & 0x3FFF) | 0x8000, // y = [8,9,a,b]
             ((uint64_t)(r3 & 0xFFFF) << 32) | r4);

    return buffer;
}

#endif // ESP32 || ESP_PLATFORM
