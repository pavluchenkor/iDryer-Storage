/**
 * @file moonraker_client.h
 * @brief Клиент Moonraker (Klipper) через WebSocket JSON-RPC.
 *
 * Для iHeater — основной источник целевой температуры камеры через
 * `gcode_macro VIRTUAL_CHAMBER.target` (проверенный подход, см.
 * `docs/iHeater-link/virtual_chamber_guide.md`).
 *
 * Подключение: `ws://host:port/websocket` либо `wss://...`, поверх —
 * JSON-RPC 2.0. LINK подписывается на объекты Klipper (включая
 * кастомный `gcode_macro VIRTUAL_CHAMBER`) и обрабатывает
 * `notify_status_update` notifications.
 *
 * Runtime-состояние публикуется в `integrations/status.moonraker`
 * через `LinkIntegrationsManager`. Для iHeater-прошивки ключевой
 * интерфейс — `setChamberTargetCallback`, который получает целевую
 * температуру камеры.
 *
 * @see docs/ru/07-features/06-moonraker-printer.md
 */

#pragma once

#include "link_integrations_types.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <Arduino.h>
#include <ArduinoJson.h>   // JsonObjectConst используется в сигнатуре applyStatusUpdate
#include <WebSocketsClient.h>
#include <functional>

namespace idryer {
namespace cloud {

/// Runtime-состояние `MoonrakerClient`.
enum class MoonrakerConnectionState : uint8_t
{
    Disabled,       // configure() не вызывался или shutdown()
    Idle,           // настроен, но enabled=false
    Connecting,     // TCP/WS handshake в процессе / reconnect
    Connected,      // WS открыт, JSON-RPC подписка активна
    Error,          // фатальная ошибка (auth и т.п.)
};

const char* moonrakerConnectionStateToString(MoonrakerConnectionState value);

/// Снимок наиболее полезных полей статуса из Moonraker. Заполняется
/// частично по мере поступления notifications — поля, по которым
/// Moonraker ещё не прислал данных, остаются в дефолте.
struct MoonrakerStatus
{
    float    chamberTarget          = 0.0f;   // VIRTUAL_CHAMBER.target
    float    chamberTemperature     = 0.0f;   // VIRTUAL_CHAMBER.temperature (валидна при hasSensor)
    bool     chamberHasSensor       = false;  // VIRTUAL_CHAMBER.has_sensor (1 → true)
    bool     virtualChamberAvailable = false; // пришёл ли снэпшот объекта макроса
    char     printerState[16]       = {0};    // print_stats.state: "standby"/"printing"/...
    float    progress               = 0.0f;   // display_status.progress * 100 (0..100)
    float    nozzleTemp             = 0.0f;
    float    nozzleTarget           = 0.0f;
    float    bedTemp                = 0.0f;
    float    bedTarget              = 0.0f;
    uint32_t printDurationSeconds   = 0;
    char     filename[96]           = {0};
};

/// Компактный срез VIRTUAL_CHAMBER для прошивки iHeater. Передаётся в
/// `VirtualChamberCallback` при каждом изменении любого из полей.
struct VirtualChamberData
{
    bool  available   = false;  // объект `gcode_macro VIRTUAL_CHAMBER` виден в Klipper
    bool  hasSensor   = false;  // установлено `variable_has_sensor: 1` в макросе
    float target      = 0.0f;   // °C (0 = выкл)
    float temperature = 0.0f;   // °C (валидно только при hasSensor == true)
};

class MoonrakerClient
{
public:
    using StateChangeCallback     = std::function<void(MoonrakerConnectionState)>;
    using ChamberTargetCallback   = std::function<void(float target, bool available)>;
    using VirtualChamberCallback  = std::function<void(const VirtualChamberData&)>;
    using StatusChangeCallback    = std::function<void(const MoonrakerStatus&)>;

    MoonrakerClient();
    ~MoonrakerClient();

    // Жизненный цикл ---------------------------------------------------------

    void configure(const MoonrakerConfig& cfg);
    void shutdown();
    void loop();

    // Наблюдение -------------------------------------------------------------

    MoonrakerConnectionState state() const { return state_; }
    bool isConnected() const { return state_ == MoonrakerConnectionState::Connected; }
    const char* lastError() const { return lastError_; }

    /// Последний известный снимок. Безопасно читать в любом состоянии.
    const MoonrakerStatus& status() const { return status_; }

    // Callbacks --------------------------------------------------------------

    /// Изменение общего connection-state (Disabled/Connecting/Connected/Error).
    void setStateChangeCallback(StateChangeCallback cb) { stateCallback_ = std::move(cb); }

    /// Legacy-колбэк: фаер при изменении `target` или `available`.
    /// Оставлен для обратной совместимости. Для нового кода используй
    /// `setVirtualChamberCallback` — он отдаёт все поля macro (target +
    /// temperature + has_sensor).
    /// @param target    целевая температура камеры (°C); 0 = выключено.
    /// @param available `true`, если Moonraker вернул объект `gcode_macro VIRTUAL_CHAMBER`.
    void setChamberTargetCallback(ChamberTargetCallback cb) { chamberCallback_ = std::move(cb); }

    /// Главный потребительский колбэк для iHeater (предпочтительный).
    /// Вызывается при КАЖДОМ изменении `target` / `temperature` / `has_sensor`
    /// / `available` в `gcode_macro VIRTUAL_CHAMBER` — включая начальный
    /// snapshot после подключения. Передаётся полный `VirtualChamberData`.
    void setVirtualChamberCallback(VirtualChamberCallback cb) { vcCallback_ = std::move(cb); }

    /// Колбэк при обновлении остальных полей статуса (печать, температуры, filename).
    /// Фьюзингуется — публикуется пакетами, не на каждый notify.
    void setStatusChangeCallback(StatusChangeCallback cb) { statusCallback_ = std::move(cb); }

private:
    static constexpr uint32_t kReconnectMinMs = 1000;
    static constexpr uint32_t kReconnectMaxMs = 60000;
    static constexpr const char* kWsPath = "/websocket";

    // Обработчик событий WebSocketsClient.
    void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);

    // Обработка одного текстового JSON-RPC сообщения от Moonraker.
    void handleJsonRpcMessage(const char* text, size_t length);

    // Применение status-объекта из `notify_status_update` или первого snapshot.
    void applyStatusUpdate(const JsonObjectConst& statusObj);

    // Отправка подписки сразу после connect.
    void sendSubscribe();

    void setState(MoonrakerConnectionState newState);
    void setError(const char* message);

    MoonrakerConfig          cfg_{};
    bool                     configValid_ = false;
    MoonrakerConnectionState state_       = MoonrakerConnectionState::Disabled;
    char                     lastError_[96] = {0};

    WebSocketsClient         ws_;
    uint32_t                 reconnectBackoffMs_   = kReconnectMinMs;
    uint32_t                 nextConnectAttemptMs_ = 0;
    uint32_t                 nextRpcId_            = 1;

    MoonrakerStatus          status_{};

    StateChangeCallback       stateCallback_;
    ChamberTargetCallback     chamberCallback_;
    VirtualChamberCallback    vcCallback_;
    StatusChangeCallback      statusCallback_;
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
