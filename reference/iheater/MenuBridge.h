/**
 * @file MenuBridge.h
 * @brief Мост между меню (NVS-backed MenuState + g_menu_cache) и MQTT-порталом.
 *
 * Задачи:
 *   - Инициализация NVS namespace меню, загрузка сохранённых значений, синхронизация
 *     с `g_menu_cache` (который использует `menu_buildFullJson()` для отдачи config).
 *   - Публикация полного меню (`{v, menu:[...]}`) в MQTT топик `config`
 *     в ответ на `commands/get_config`.
 *   - Применение одиночных правок (`commands/set` → `menu_apply_by_bind` → NVS persist),
 *     обновление `g_menu_cache` и повторная публикация полного config.
 */

#pragma once

#include <ArduinoJson.h>
#include <functional>

// Forward declaration — чтобы не тянуть весь mqtt_client.h в публичный заголовок.
class MqttClient;

namespace iheaterlink {

/// Идентификатор выбранного в меню «ПОДКЛЮЧЕНИЯ» источника target температуры.
/// Соответствует значению toggle-поля (bambu_en / moon_en / ha_en).
enum class ActiveConnection : uint8_t { None = 0, Bambu, Moonraker, Ha };

/// Колбэк на смену активного ПОДКЛЮЧЕНИЯ — вызывается из MenuBridge при
/// изменении toggle-ов или при старте (синхронизация с сохранённым NVS).
using ActiveConnectionCallback = std::function<void(ActiveConnection)>;

class MenuBridge {
public:
    explicit MenuBridge(MqttClient* mqtt) : mqtt_(mqtt) {}

    /// Зарегистрировать колбэк на смену активного ПОДКЛЮЧЕНИЯ.
    /// Будет вызван один раз из begin() со стартовым значением и далее
    /// при каждом applySetCommand, которое поменяло bambu_en/moon_en/ha_en.
    void setActiveConnectionCallback(ActiveConnectionCallback cb) { activeCb_ = std::move(cb); }

    /// Открыть NVS, подтянуть дефолты, загрузить сохранённые значения,
    /// синхронизировать MenuState → g_menu_cache.
    /// Безопасно вызывать повторно (no-op после первого успеха).
    void begin();

    /// Построить полный JSON меню и опубликовать в MQTT `config`.
    /// Возвращает true если публикация успешна.
    bool publishFullConfig();

    /// Применить одиночное изменение значения.
    /// Ожидаемый JSON: {"id":<int>, "val":<num|bool>} или {"bind":"<name>", "val":<num|bool>}.
    /// Обновляет MenuState, g_menu_cache и NVS; публикует обновлённый config.
    /// @return true если правка применена.
    bool applySetCommand(JsonObjectConst data);

private:
    /// Прочитать все biнды из MenuState и записать в g_menu_cache.
    /// Вызывается после loadFromNVS() и после каждого apply.
    void syncStateToCache();

    /// Прочитать текущее состояние bambu_en/moon_en/ha_en и вернуть
    /// активное подключение (или None если все выключены).
    ActiveConnection currentActive() const;

    /// Вызвать activeCb_ если значение изменилось с прошлого вызова.
    void emitActiveIfChanged();

    MqttClient* mqtt_ = nullptr;
    bool nvsReady_ = false;
    ActiveConnectionCallback activeCb_;
    ActiveConnection lastActive_ = ActiveConnection::None;
};

} // namespace iheaterlink
