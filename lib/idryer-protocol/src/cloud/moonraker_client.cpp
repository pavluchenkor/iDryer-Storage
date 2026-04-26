/**
 * @file moonraker_client.cpp
 * @brief Реализация клиента Moonraker (Klipper) через WebSocket JSON-RPC.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "moonraker_client.h"
#include "../hal/hal_types.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

namespace idryer {
namespace cloud {

const char* moonrakerConnectionStateToString(MoonrakerConnectionState value)
{
    switch (value)
    {
    case MoonrakerConnectionState::Disabled:   return "disabled";
    case MoonrakerConnectionState::Idle:       return "idle";
    case MoonrakerConnectionState::Connecting: return "connecting";
    case MoonrakerConnectionState::Connected:  return "connected";
    case MoonrakerConnectionState::Error:      return "error";
    }
    return "disabled";
}

// =============================================================================
// Constructor / destructor
// =============================================================================

MoonrakerClient::MoonrakerClient()
{
    // Регистрируем обработчик событий. Лямбда захватывает `this`.
    ws_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->onWebSocketEvent(type, payload, length);
    });
}

MoonrakerClient::~MoonrakerClient()
{
    shutdown();
}

// =============================================================================
// configure / shutdown
// =============================================================================

void MoonrakerClient::configure(const MoonrakerConfig& cfg)
{
    bool identical =
        configValid_
        && cfg.enabled == cfg_.enabled
        && strcmp(cfg.host, cfg_.host) == 0
        && cfg.port == cfg_.port
        && cfg.ssl  == cfg_.ssl
        && strcmp(cfg.apiKey, cfg_.apiKey) == 0;

    if (identical) return;

    // Меняется конфиг — всё сносим и поднимаем заново.
    ws_.disconnect();

    cfg_ = cfg;
    configValid_ = cfg_.configured();
    status_ = MoonrakerStatus{};

    if (!cfg_.enabled || !configValid_) {
        setState(MoonrakerConnectionState::Idle);
        if (!configValid_) setError("configuration incomplete (need host)");
        else               setError("");
        return;
    }

    // Подготовка URL. `WebSocketsClient::begin(host, port, path)` работает
    // и с hostname, и с IP. `beginSSL` — для wss.
    // Путь: добавляем опциональный `?token=...` для Moonraker auth.
    char path[96];
    if (cfg_.apiKey[0] != '\0') {
        snprintf(path, sizeof(path), "%s?token=%s", kWsPath, cfg_.apiKey);
    } else {
        snprintf(path, sizeof(path), "%s", kWsPath);
    }

    if (cfg_.ssl) {
        ws_.beginSSL(cfg_.host, cfg_.port, path);
    } else {
        ws_.begin(cfg_.host, cfg_.port, path);
    }

    // Убираем дефолтный `Origin: file://`, который добавляет WebSocketsClient.
    // Moonraker проверяет Origin против `cors_domains`, и `file://` там обычно
    // нет → handshake получает 403 Forbidden. Без Origin Moonraker проходит
    // по trusted_clients и WS upgrade работает.
    ws_.setExtraHeaders("");

    // Включаем собственный reconnect WebSocketsClient — он работает с
    // интервалом, мы поверх него ведём state machine.
    ws_.setReconnectInterval(reconnectBackoffMs_);

    reconnectBackoffMs_ = kReconnectMinMs;
    nextConnectAttemptMs_ = millis();
    setState(MoonrakerConnectionState::Connecting);
    setError("");

    HAL_LOG_INFO("MOON", "configure: %s://%s:%u%s",
                 cfg_.ssl ? "wss" : "ws",
                 cfg_.host, cfg_.port, kWsPath);
}

void MoonrakerClient::shutdown()
{
    ws_.disconnect();
    configValid_ = false;
    setState(MoonrakerConnectionState::Disabled);
}

// =============================================================================
// loop
// =============================================================================

void MoonrakerClient::loop()
{
    if (state_ == MoonrakerConnectionState::Disabled
        || state_ == MoonrakerConnectionState::Idle)
    {
        return;
    }
    ws_.loop();
}

// =============================================================================
// WebSocket events
// =============================================================================

void MoonrakerClient::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
    switch (type)
    {
    case WStype_CONNECTED:
        HAL_LOG_INFO("MOON", "ws connected: %s", payload ? (const char*)payload : "");
        setState(MoonrakerConnectionState::Connected);
        setError("");
        reconnectBackoffMs_ = kReconnectMinMs;
        ws_.setReconnectInterval(reconnectBackoffMs_);
        sendSubscribe();
        break;

    case WStype_DISCONNECTED:
        HAL_LOG_WARN("MOON", "ws disconnected (backoff=%ums)", reconnectBackoffMs_);
        setState(MoonrakerConnectionState::Connecting);
        // Рост backoff. WebSocketsClient сам повторит попытку.
        reconnectBackoffMs_ *= 2;
        if (reconnectBackoffMs_ > kReconnectMaxMs) reconnectBackoffMs_ = kReconnectMaxMs;
        ws_.setReconnectInterval(reconnectBackoffMs_);
        break;

    case WStype_TEXT:
        if (payload && length > 0) {
            handleJsonRpcMessage((const char*)payload, length);
        }
        break;

    case WStype_ERROR:
        setError("ws error");
        HAL_LOG_WARN("MOON", "ws error");
        break;

    default:
        // PING/PONG/BIN и прочее игнорируем.
        break;
    }
}

// =============================================================================
// JSON-RPC
// =============================================================================

void MoonrakerClient::sendSubscribe()
{
    // printer.objects.subscribe — получаем начальный snapshot + дальше
    // приходят `notify_status_update` при изменениях любого из полей.
    StaticJsonDocument<512> doc;
    doc["jsonrpc"] = "2.0";
    doc["method"]  = "printer.objects.subscribe";

    JsonObject params  = doc.createNestedObject("params");
    JsonObject objects = params.createNestedObject("objects");

    // Ключевой объект для iHeater — подписываемся на три конкретных поля,
    // чтобы не тянуть лишние переменные макроса и держать notify мелким.
    JsonArray vcFields = objects.createNestedArray("gcode_macro VIRTUAL_CHAMBER");
    vcFields.add("target");
    vcFields.add("temperature");
    vcFields.add("has_sensor");

    // Полезные для UI (iDryer/iHeater):
    objects["print_stats"]      = nullptr;
    objects["virtual_sdcard"]   = nullptr;
    objects["display_status"]   = nullptr;

    // Для температур — только нужные поля (снизим трафик):
    JsonArray extruderFields = objects.createNestedArray("extruder");
    extruderFields.add("temperature");
    extruderFields.add("target");

    JsonArray bedFields = objects.createNestedArray("heater_bed");
    bedFields.add("temperature");
    bedFields.add("target");

    doc["id"] = nextRpcId_++;

    String out;
    serializeJson(doc, out);
    ws_.sendTXT(out);
    HAL_LOG_INFO("MOON", "subscribe sent (%u bytes)", (unsigned)out.length());
}

void MoonrakerClient::handleJsonRpcMessage(const char* text, size_t length)
{
    // Разбираем JSON-RPC ответ или notification. Берём относительно большой
    // размер — snapshot подписки может быть объёмным (температуры, статус).
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, text, length);
    if (err) {
        HAL_LOG_WARN("MOON", "json parse err: %s", err.c_str());
        return;
    }

    // Notification: `method` + `params`, без `id`.
    const char* method = doc["method"] | (const char*)nullptr;
    if (method && strcmp(method, "notify_status_update") == 0) {
        // params = [ { "gcode_macro VIRTUAL_CHAMBER": {...}, ... }, eventtime ]
        JsonArrayConst params = doc["params"].as<JsonArrayConst>();
        if (!params.isNull() && params.size() >= 1) {
            JsonObjectConst statusObj = params[0].as<JsonObjectConst>();
            if (!statusObj.isNull()) {
                applyStatusUpdate(statusObj);
            }
        }
        return;
    }

    // RPC response (на subscribe): ожидаем `result.status`.
    if (doc.containsKey("result")) {
        JsonObjectConst result = doc["result"].as<JsonObjectConst>();
        if (!result.isNull()) {
            JsonObjectConst statusObj = result["status"].as<JsonObjectConst>();
            if (!statusObj.isNull()) {
                applyStatusUpdate(statusObj);
            }
        }
        return;
    }

    // error-ответ
    if (doc.containsKey("error")) {
        const char* msg = doc["error"]["message"] | "rpc error";
        HAL_LOG_WARN("MOON", "rpc error: %s", msg);
        setError(msg);
    }
}

void MoonrakerClient::applyStatusUpdate(const JsonObjectConst& statusObj)
{
    bool chamberChanged = false;   // изменилось хотя бы одно VIRTUAL_CHAMBER-поле
    bool vcStructuralChange = false; // изменилось target / has_sensor / available (это попадает в status)
    bool otherChanged   = false;

    // VIRTUAL_CHAMBER (target / temperature / has_sensor).
    JsonObjectConst vc = statusObj["gcode_macro VIRTUAL_CHAMBER"].as<JsonObjectConst>();
    if (!vc.isNull()) {
        if (!status_.virtualChamberAvailable) {
            status_.virtualChamberAvailable = true;
            vcStructuralChange = true;
            chamberChanged = true;
        }

        if (vc.containsKey("target")) {
            float target = vc["target"].as<float>();
            if (target != status_.chamberTarget) {
                status_.chamberTarget = target;
                chamberChanged = true;
                vcStructuralChange = true;
            }
        }

        if (vc.containsKey("temperature")) {
            float temp = vc["temperature"].as<float>();
            if (temp != status_.chamberTemperature) {
                status_.chamberTemperature = temp;
                chamberChanged = true;
                // temperature не триггерит vcStructuralChange — меняется
                // часто, в integrations/status уйдёт только по таймеру или
                // вместе с другими значимыми изменениями.
            }
        }

        if (vc.containsKey("has_sensor")) {
            // Klipper отдаёт int (0/1) — берём как bool.
            bool hs = vc["has_sensor"].as<int>() != 0;
            if (hs != status_.chamberHasSensor) {
                status_.chamberHasSensor = hs;
                chamberChanged = true;
                vcStructuralChange = true;
            }
        }
    }

    // print_stats.state + filename + print_duration
    JsonObjectConst ps = statusObj["print_stats"].as<JsonObjectConst>();
    if (!ps.isNull()) {
        const char* s = ps["state"] | (const char*)nullptr;
        if (s) {
            if (strncmp(status_.printerState, s, sizeof(status_.printerState) - 1) != 0) {
                strncpy(status_.printerState, s, sizeof(status_.printerState) - 1);
                status_.printerState[sizeof(status_.printerState) - 1] = '\0';
                otherChanged = true;
            }
        }
        const char* fn = ps["filename"] | (const char*)nullptr;
        if (fn) {
            if (strncmp(status_.filename, fn, sizeof(status_.filename) - 1) != 0) {
                strncpy(status_.filename, fn, sizeof(status_.filename) - 1);
                status_.filename[sizeof(status_.filename) - 1] = '\0';
                otherChanged = true;
            }
        }
        if (ps.containsKey("print_duration")) {
            uint32_t d = (uint32_t)(ps["print_duration"].as<float>());
            if (d != status_.printDurationSeconds) {
                status_.printDurationSeconds = d;
                otherChanged = true;
            }
        }
    }

    // display_status.progress (0..1) → progress в 0..100
    JsonObjectConst ds = statusObj["display_status"].as<JsonObjectConst>();
    if (!ds.isNull() && ds.containsKey("progress")) {
        float p = ds["progress"].as<float>() * 100.0f;
        if (p != status_.progress) {
            status_.progress = p;
            otherChanged = true;
        }
    }

    // extruder / heater_bed
    JsonObjectConst ext = statusObj["extruder"].as<JsonObjectConst>();
    if (!ext.isNull()) {
        if (ext.containsKey("temperature")) {
            float t = ext["temperature"].as<float>();
            if (t != status_.nozzleTemp) { status_.nozzleTemp = t; otherChanged = true; }
        }
        if (ext.containsKey("target")) {
            float t = ext["target"].as<float>();
            if (t != status_.nozzleTarget) { status_.nozzleTarget = t; otherChanged = true; }
        }
    }
    JsonObjectConst bed = statusObj["heater_bed"].as<JsonObjectConst>();
    if (!bed.isNull()) {
        if (bed.containsKey("temperature")) {
            float t = bed["temperature"].as<float>();
            if (t != status_.bedTemp) { status_.bedTemp = t; otherChanged = true; }
        }
        if (bed.containsKey("target")) {
            float t = bed["target"].as<float>();
            if (t != status_.bedTarget) { status_.bedTarget = t; otherChanged = true; }
        }
    }

    // Уведомляем подписчиков.
    // Legacy-колбэк: только при структурных изменениях target/available
    // (его потребители не ожидают поток обновлений на temperature).
    if (vcStructuralChange && chamberCallback_) {
        chamberCallback_(status_.chamberTarget, status_.virtualChamberAvailable);
    }

    // Новый колбэк: полный срез VIRTUAL_CHAMBER, на КАЖДОЕ изменение.
    // iHeater использует его для PID — нужны частые значения temperature.
    if (chamberChanged && vcCallback_) {
        VirtualChamberData data;
        data.available   = status_.virtualChamberAvailable;
        data.hasSensor   = status_.chamberHasSensor;
        data.target      = status_.chamberTarget;
        data.temperature = status_.chamberTemperature;
        vcCallback_(data);
    }

    if ((chamberChanged || otherChanged) && statusCallback_) {
        statusCallback_(status_);
    }
}

// =============================================================================
// Helpers
// =============================================================================

void MoonrakerClient::setState(MoonrakerConnectionState newState)
{
    if (state_ == newState) return;
    MoonrakerConnectionState old = state_;
    state_ = newState;
    HAL_LOG_INFO("MOON", "state: %s -> %s",
                 moonrakerConnectionStateToString(old),
                 moonrakerConnectionStateToString(newState));
    if (stateCallback_) stateCallback_(newState);
}

void MoonrakerClient::setError(const char* message)
{
    if (!message) message = "";
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
