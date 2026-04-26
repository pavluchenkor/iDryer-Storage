/**
 * @file bambu_client.cpp
 * @brief Реализация клиента Bambu Lab LAN MQTT (Фаза 2 — Writer).
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "bambu_client.h"
#include "../hal/hal_types.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace idryer {
namespace cloud {

const char* bambuConnectionStateToString(BambuConnectionState value)
{
    switch (value)
    {
    case BambuConnectionState::Disabled:   return "disabled";
    case BambuConnectionState::Idle:       return "idle";
    case BambuConnectionState::Connecting: return "connecting";
    case BambuConnectionState::Connected:  return "connected";
    case BambuConnectionState::Error:      return "error";
    }
    return "disabled";
}

namespace {

// Нормализация цвета: 6 hex → добавить "FF" в конец для альфа-канала.
// Возвращает указатель на `outBuf` (8 символов + '\0') или nullptr при ошибке.
const char* normalizeColor(const char* in, char* outBuf, size_t bufSize)
{
    if (!in || !outBuf || bufSize < 9) return nullptr;
    size_t len = strlen(in);

    if (len == 8) {
        memcpy(outBuf, in, 8);
        outBuf[8] = '\0';
        return outBuf;
    }
    if (len == 6) {
        memcpy(outBuf, in, 6);
        outBuf[6] = 'F';
        outBuf[7] = 'F';
        outBuf[8] = '\0';
        return outBuf;
    }
    return nullptr;  // невалидно
}

} // namespace

// =============================================================================
// Constructor / destructor
// =============================================================================

// Статический указатель — PubSubClient принимает только сырой function pointer,
// поэтому маршрутизируем через одиночный инстанс. В рамках библиотеки
// существует один BambuClient (в LinkIntegrationsManager).
BambuClient* BambuClient::instance_ = nullptr;

void BambuClient::staticMqttCallback(char* topic, uint8_t* payload, unsigned int length)
{
    if (instance_) {
        instance_->handleReportMessage(topic, payload, length);
    }
}

BambuClient::BambuClient()
{
    // PubSubClient создан с tlsClient_ в init-list. Параметры брокера
    // (host/port/callback) выставляем в attemptConnect.
    mqttClient_.setBufferSize(kTlsBufferSize);
    mqttClient_.setCallback(&BambuClient::staticMqttCallback);
    instance_ = this;
}

BambuClient::~BambuClient()
{
    shutdown();
    if (instance_ == this) instance_ = nullptr;
}

void BambuClient::setMode(BambuMode mode)
{
    if (mode_ == mode) return;
    mode_ = mode;
    HAL_LOG_INFO("BAMBU", "mode: %s",
                 mode == BambuMode::Writer ? "writer" : "reader");
    // Если уже подключены — сбрасываем коннект, чтобы при следующем
    // connect применилась новая стратегия (subscribe vs no-subscribe).
    if (mqttClient_.connected()) {
        mqttClient_.disconnect();
    }
    // state перейдёт в Connecting в ближайшем loop()
}

// =============================================================================
// configure / shutdown
// =============================================================================

void BambuClient::configure(const BambuConfig& cfg)
{
    // Если ничего не меняется — no-op.
    bool identical =
        configValid_
        && cfg.enabled == cfg_.enabled
        && strcmp(cfg.ip, cfg_.ip) == 0
        && strcmp(cfg.serial, cfg_.serial) == 0
        && strcmp(cfg.lanAccessCode, cfg_.lanAccessCode) == 0;

    if (identical) {
        return;
    }

    // Конфигурация изменилась или только подаётся — дисконнектимся и
    // поднимаем с нуля.
    if (mqttClient_.connected()) {
        mqttClient_.disconnect();
    }

    cfg_ = cfg;
    configValid_ = cfg_.configured();

    // Топики
    snprintf(requestTopic_, sizeof(requestTopic_), "device/%s/request", cfg_.serial);
    snprintf(reportTopic_,  sizeof(reportTopic_),  "device/%s/report",  cfg_.serial);

    if (!cfg_.enabled || !configValid_) {
        setState(BambuConnectionState::Idle);
        if (!configValid_) setError("configuration incomplete (need ip/serial/lanAccessCode)");
        else               setError("");
        return;
    }

    // TLS: self-signed у Bambu, `setInsecure` — единственный рабочий путь
    // (подтверждено в OpenSpool).
    tlsClient_.setInsecure();

    // Server/callback
    mqttClient_.setServer(cfg_.ip, kBambuPort);
    mqttClient_.setKeepAlive(30);

    // Сбрасываем backoff и запрашиваем коннект сразу
    reconnectBackoffMs_ = kReconnectMinMs;
    nextConnectAttemptMs_ = millis();
    setState(BambuConnectionState::Connecting);
    setError("");

    HAL_LOG_INFO("BAMBU", "configure: ip=%s serial=%s port=%u",
                 cfg_.ip, cfg_.serial, kBambuPort);
}

void BambuClient::shutdown()
{
    if (mqttClient_.connected()) {
        mqttClient_.disconnect();
    }
    configValid_ = false;
    setState(BambuConnectionState::Disabled);
}

// =============================================================================
// loop: reconnect + pubsub service
// =============================================================================

void BambuClient::loop()
{
    if (state_ == BambuConnectionState::Disabled
        || state_ == BambuConnectionState::Idle)
    {
        return;
    }

    if (mqttClient_.connected()) {
        mqttClient_.loop();
        return;
    }

    // Иначе — пытаемся (пере)подключиться с учётом backoff.
    uint32_t now = millis();
    if (now < nextConnectAttemptMs_) {
        return;
    }

    if (state_ != BambuConnectionState::Connecting) {
        setState(BambuConnectionState::Connecting);
    }
    attemptConnect();
}

void BambuClient::attemptConnect()
{
    if (WiFi.status() != WL_CONNECTED) {
        setError("no wifi");
        // Не экспоненциально растим — попробуем снова скоро, когда WiFi появится
        nextConnectAttemptMs_ = millis() + 2000;
        return;
    }

    // Client ID: хватит уникальности с serial принтера + постфиксом
    char clientId[48];
    snprintf(clientId, sizeof(clientId), "idryer_%s", cfg_.serial);

    HAL_LOG_INFO("BAMBU", "connect attempt: %s:%u (user=%s)",
                 cfg_.ip, kBambuPort, kBambuUser);

    bool ok = mqttClient_.connect(clientId, kBambuUser, cfg_.lanAccessCode);

    if (ok) {
        setState(BambuConnectionState::Connected);
        setError("");
        reconnectBackoffMs_ = kReconnectMinMs;  // сбрасываем
        HAL_LOG_INFO("BAMBU", "connected");

        // Reader-режим: подписываемся на report + сразу просим полный snapshot.
        if (mode_ == BambuMode::Reader) {
            if (mqttClient_.subscribe(reportTopic_)) {
                HAL_LOG_INFO("BAMBU", "subscribed to %s", reportTopic_);
            } else {
                HAL_LOG_WARN("BAMBU", "subscribe failed on %s", reportTopic_);
            }
            requestPushAll();
        }
        return;
    }

    // Не получилось — растим backoff
    int rc = mqttClient_.state();
    char buf[96];
    snprintf(buf, sizeof(buf), "connect failed rc=%d", rc);
    setError(buf);
    HAL_LOG_WARN("BAMBU", "%s (backoff=%ums)", buf, reconnectBackoffMs_);

    nextConnectAttemptMs_ = millis() + reconnectBackoffMs_;
    reconnectBackoffMs_ *= 2;
    if (reconnectBackoffMs_ > kReconnectMaxMs) reconnectBackoffMs_ = kReconnectMaxMs;
}

// =============================================================================
// applyFilament — Writer-path
// =============================================================================

BambuApplyResult BambuClient::applyFilament(const BambuApplyPayload& payload)
{
    BambuApplyResult result{};

    if (state_ != BambuConnectionState::Connected) {
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "not connected (state=%s)", bambuConnectionStateToString(state_));
        return result;
    }

    if (!payload.valid()) {
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "payload invalid (need trayType/colorHex/nozzleTemp/trayInfoIdx)");
        return result;
    }

    // Резолв дефолтов
    uint8_t amsId  = (payload.amsId  == kBambuApplyAmsFromConfig)  ? cfg_.defaultAmsId  : payload.amsId;
    uint8_t trayId = (payload.trayId == kBambuApplyTrayFromConfig) ? cfg_.defaultTrayId : payload.trayId;

    // Нормализация цвета: 8 hex обязательно (RGBA).
    char color[9];
    const char* normalized = normalizeColor(payload.colorHex, color, sizeof(color));
    if (!normalized) {
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "bad colorHex '%s' (need 6 or 8 hex)", payload.colorHex);
        return result;
    }

    // Сборка JSON
    StaticJsonDocument<512> doc;
    JsonObject print = doc.createNestedObject("print");
    print["sequence_id"]      = "1";
    print["command"]          = "ams_filament_setting";
    print["ams_id"]           = amsId;
    print["tray_id"]          = trayId;
    print["tray_info_idx"]    = payload.trayInfoIdx;
    print["tray_color"]       = normalized;
    print["nozzle_temp_min"]  = payload.nozzleTempMin;
    print["nozzle_temp_max"]  = payload.nozzleTempMax;
    print["tray_type"]        = payload.trayType;
    print["setting_id"]       = payload.settingId;  // обычно ""

    String out;
    serializeJson(doc, out);

    HAL_LOG_INFO("BAMBU", "applyFilament: type=%s color=%s temp=%u..%u ams=%u tray=%u uid=%s",
                 payload.trayType, normalized,
                 payload.nozzleTempMin, payload.nozzleTempMax,
                 amsId, trayId,
                 payload.uid[0] ? payload.uid : "(none)");

    if (!publishJsonDoc(requestTopic_, out)) {
        snprintf(result.errorMessage, sizeof(result.errorMessage),
                 "publish failed (broker busy or oversized payload)");
        return result;
    }

    result.success = true;
    return result;
}

bool BambuClient::publishJsonDoc(const char* topic, const String& payload)
{
    // PubSubClient::publish на большой payload возвращает false, если
    // внутренний буфер мал — у нас 4096, JSON ~300 байт, ок.
    return mqttClient_.publish(topic, payload.c_str());
}

// =============================================================================
// Reader-path: pushall + handleReportMessage
// =============================================================================

void BambuClient::requestPushAll()
{
    // Просим принтер выплюнуть полный отчёт однократно.
    StaticJsonDocument<128> doc;
    JsonObject pushing = doc.createNestedObject("pushing");
    pushing["sequence_id"] = "1";
    pushing["command"]     = "pushall";

    String out;
    serializeJson(doc, out);
    publishJsonDoc(requestTopic_, out);
    HAL_LOG_DEBUG("BAMBU", "pushall requested");
}

namespace {

// Безопасное копирование C-строки в фикс-буфер
void copyCStr(const char* src, char* dst, size_t dstSize)
{
    if (!dst || dstSize == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dstSize) len = dstSize - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

} // namespace

void BambuClient::handleReportMessage(const char* topic, const uint8_t* payload, unsigned int length)
{
    (void)topic;  // у нас одна подписка — proверять не нужно

    // Bambu отправляет JSON такого вида:
    //   { "print": { "gcode_state":"RUNNING", "mc_percent":42, ... } }
    // или полный отчёт (на pushall).
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        HAL_LOG_WARN("BAMBU", "report parse err: %s", err.c_str());
        return;
    }

    JsonObjectConst print = doc["print"].as<JsonObjectConst>();
    if (print.isNull()) return;

    bool changed = false;

    if (print.containsKey("gcode_state")) {
        const char* s = print["gcode_state"].as<const char*>();
        if (s) {
            if (strcmp(printerStatus_.gcodeState, s) != 0) {
                copyCStr(s, printerStatus_.gcodeState, sizeof(printerStatus_.gcodeState));
                changed = true;
            }
        }
    }

    if (print.containsKey("mc_percent")) {
        uint8_t p = (uint8_t)print["mc_percent"].as<int>();
        if (p != printerStatus_.progressPercent) {
            printerStatus_.progressPercent = p;
            changed = true;
        }
    }

    if (print.containsKey("mc_remaining_time")) {
        // Bambu отдаёт в **минутах** (наблюдаемо в OpenBambuAPI).
        uint32_t minutes = (uint32_t)print["mc_remaining_time"].as<int>();
        uint32_t sec = minutes * 60;
        if (sec != printerStatus_.remainingSeconds) {
            printerStatus_.remainingSeconds = sec;
            changed = true;
        }
    }

    if (print.containsKey("layer_num")) {
        uint16_t v = (uint16_t)print["layer_num"].as<int>();
        if (v != printerStatus_.currentLayer) { printerStatus_.currentLayer = v; changed = true; }
    }
    if (print.containsKey("total_layer_num")) {
        uint16_t v = (uint16_t)print["total_layer_num"].as<int>();
        if (v != printerStatus_.totalLayers) { printerStatus_.totalLayers = v; changed = true; }
    }

    if (print.containsKey("nozzle_temper")) {
        float t = print["nozzle_temper"].as<float>();
        if (t != printerStatus_.nozzleTemp) { printerStatus_.nozzleTemp = t; changed = true; }
    }
    if (print.containsKey("nozzle_target_temper")) {
        float t = print["nozzle_target_temper"].as<float>();
        if (t != printerStatus_.nozzleTarget) { printerStatus_.nozzleTarget = t; changed = true; }
    }
    if (print.containsKey("bed_temper")) {
        float t = print["bed_temper"].as<float>();
        if (t != printerStatus_.bedTemp) { printerStatus_.bedTemp = t; changed = true; }
    }
    if (print.containsKey("bed_target_temper")) {
        float t = print["bed_target_temper"].as<float>();
        if (t != printerStatus_.bedTarget) { printerStatus_.bedTarget = t; changed = true; }
    }
    if (print.containsKey("chamber_temper")) {
        float t = print["chamber_temper"].as<float>();
        if (t != printerStatus_.chamberTemp) { printerStatus_.chamberTemp = t; changed = true; }
    }
    if (print.containsKey("chamber_target")) {
        float t = print["chamber_target"].as<float>();
        if (t != printerStatus_.chamberTarget) { printerStatus_.chamberTarget = t; changed = true; }
    }

    // Активный трей определяется через tray_now (только если поле присутствует в сообщении).
    // Полная формула — ha-bambulab models.py#L2775:
    //   255        → нет активного трея
    //   254        → внешняя катушка (vt_tray)
    //   >= 80      → AMS HT (ams_index = tray_now, tray_index = 0)
    //   else       → ams_index = tray_now >> 2,  tray_index = tray_now & 0x3
    JsonObjectConst ams = print["ams"].as<JsonObjectConst>();
    if (!ams.isNull() && ams.containsKey("tray_now")) {
        int trayNow = atoi(ams["tray_now"] | "255");
        const char* newTrayType    = "";
        const char* newTrayInfoIdx = nullptr;

        if (trayNow == 254) {
            JsonObjectConst vtTray = ams["vt_tray"].as<JsonObjectConst>();
            if (!vtTray.isNull()) {
                newTrayType    = vtTray["tray_type"]     | "";
                newTrayInfoIdx = vtTray["tray_info_idx"] | (const char*)nullptr;
            }
        } else if (trayNow != 255) {
            int amsIndex  = (trayNow >= 80) ? trayNow        : (trayNow >> 2);
            int trayIndex = (trayNow >= 80) ? 0              : (trayNow & 0x3);
            JsonArrayConst amsArr = ams["ams"].as<JsonArrayConst>();
            if (!amsArr.isNull()) {
                for (size_t ai = 0; ai < amsArr.size(); ++ai) {
                    if (atoi(amsArr[ai]["id"] | "255") != amsIndex) continue;
                    JsonArrayConst trays = amsArr[ai]["tray"].as<JsonArrayConst>();
                    if (!trays.isNull()) {
                        for (size_t ti = 0; ti < trays.size(); ++ti) {
                            if (atoi(trays[ti]["id"] | "255") != trayIndex) continue;
                            newTrayType    = trays[ti]["tray_type"]     | "";
                            newTrayInfoIdx = trays[ti]["tray_info_idx"] | (const char*)nullptr;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        if (strcmp(printerStatus_.trayType, newTrayType) != 0) {
            copyCStr(newTrayType, printerStatus_.trayType, sizeof(printerStatus_.trayType));
            changed = true;
        }
        if (newTrayInfoIdx && strcmp(printerStatus_.trayInfoIdx, newTrayInfoIdx) != 0) {
            copyCStr(newTrayInfoIdx, printerStatus_.trayInfoIdx, sizeof(printerStatus_.trayInfoIdx));
        } else if (!newTrayInfoIdx && printerStatus_.trayInfoIdx[0]) {
            printerStatus_.trayInfoIdx[0] = '\0';
        }
    }

    if (changed && printerStatusCallback_) {
        printerStatusCallback_(printerStatus_);
    }
}

// =============================================================================
// Helpers
// =============================================================================

void BambuClient::setState(BambuConnectionState newState)
{
    if (state_ == newState) return;
    BambuConnectionState old = state_;
    state_ = newState;
    HAL_LOG_INFO("BAMBU", "state: %s -> %s",
                 bambuConnectionStateToString(old),
                 bambuConnectionStateToString(newState));
    if (stateCallback_) stateCallback_(newState);
}

void BambuClient::setError(const char* message)
{
    if (!message) message = "";
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
