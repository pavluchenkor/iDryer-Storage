/**
 * @file link_integrations_store.h
 * @brief NVS-хранилище для LINK-интеграций (HA / Bambu / Moonraker).
 *
 * Четыре отдельных NVS namespace:
 *   - `li_ha`     — секция Home Assistant
 *   - `li_bambu`  — секция Bambu Lab
 *   - `li_moon`   — секция Moonraker / Klipper
 *   - `li_common` — общие поля (активная интеграция)
 *
 * Разделение позволяет:
 *   - стирать одну секцию (`clearHa`) без затрагивания других;
 *   - независимо версионировать структуры;
 *   - использовать короткие имена ключей (ограничение NVS 15 символов).
 *
 * @see docs/ru/07-features/03-link-integrations-overview.md
 */

#pragma once

#include "link_integrations_types.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

namespace idryer {
namespace cloud {

/// Хранилище конфигураций трёх LINK-интеграций в NVS ESP32.
/// Все методы потокобезопасны в рамках одного потока; параллельный доступ
/// из задач не поддерживается (как и `Preferences` в принципе).
class LinkIntegrationsStore
{
public:
    LinkIntegrationsStore() = default;

    /// Вызывается один раз при старте. На ESP32 NVS всегда доступна
    /// после инициализации Arduino — метод возвращает `true`.
    bool begin();

    // ------------------------------------------------------------------------
    // Home Assistant
    // ------------------------------------------------------------------------

    bool loadHa(HaConfig& out) const;
    bool saveHa(const HaConfig& value);
    void clearHa();

    // ------------------------------------------------------------------------
    // Bambu Lab
    // ------------------------------------------------------------------------

    bool loadBambu(BambuConfig& out) const;
    bool saveBambu(const BambuConfig& value);
    void clearBambu();

    // ------------------------------------------------------------------------
    // Moonraker / Klipper
    // ------------------------------------------------------------------------

    bool loadMoonraker(MoonrakerConfig& out) const;
    bool saveMoonraker(const MoonrakerConfig& value);
    void clearMoonraker();

    // ------------------------------------------------------------------------
    // Общие поля (active integration)
    // ------------------------------------------------------------------------

    bool loadCommon(CommonConfig& out) const;
    bool saveCommon(const CommonConfig& value);
    void clearCommon();

    /// Сносит все четыре секции. Используется для полного сброса.
    void clearAll();

private:
    // NVS namespace имена. ESP32 NVS ограничивает имя namespace 15 символами.
    static constexpr const char* kNamespaceHa        = "li_ha";
    static constexpr const char* kNamespaceBambu     = "li_bambu";
    static constexpr const char* kNamespaceMoonraker = "li_moon";
    static constexpr const char* kNamespaceCommon    = "li_common";
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
