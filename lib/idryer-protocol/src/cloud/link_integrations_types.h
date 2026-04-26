/**
 * @file link_integrations_types.h
 * @brief Типы и структуры для LINK-интеграций (HA / Bambu / Moonraker).
 *
 * Один транспортный контракт (MQTT `commands/link_integration`) — три
 * интеграции. Активна одновременно одна, выбор — через `ActiveIntegration`
 * в `CommonConfig`.
 *
 * @see docs/ru/07-features/03-link-integrations-overview.md
 */

#pragma once

#include <stdint.h>

namespace idryer {
namespace cloud {

/// Выбор активной LINK-интеграции (из меню MCU или локально на LINK).
/// Значения сериализуются в MQTT `integrations/status.active` как строки.
enum class ActiveIntegration : uint8_t
{
    None      = 0,
    Ha        = 1,
    Bambu     = 2,
    Moonraker = 3,
};

const char* activeIntegrationToString(ActiveIntegration value);
bool activeIntegrationFromString(const char* str, ActiveIntegration& out);

// =============================================================================
// Home Assistant config
// =============================================================================

struct HaConfig
{
    bool     enabled         = false;
    char     host[64]        = {0};
    uint16_t port            = 1883;
    char     username[33]    = {0};
    char     password[65]    = {0};
    char     discoveryPrefix[24] = {0};  // "homeassistant" по дефолту

    bool configured() const { return host[0] != '\0'; }
};

// =============================================================================
// Bambu Lab config
// =============================================================================

struct BambuConfig
{
    bool     enabled               = false;
    char     ip[16]                = {0};   // IPv4 max "255.255.255.255"
    char     serial[20]            = {0};   // Bambu serial ~15 chars
    char     lanAccessCode[16]     = {0};   // обычно 8 цифр
    uint8_t  defaultAmsId          = 255;
    uint8_t  defaultTrayId         = 254;
    bool     autoApplyOnTagDetect  = true;

    bool configured() const { return ip[0] != '\0' && serial[0] != '\0' && lanAccessCode[0] != '\0'; }
};

// =============================================================================
// Moonraker / Klipper config
// =============================================================================

struct MoonrakerConfig
{
    bool     enabled         = false;
    char     host[64]        = {0};
    uint16_t port            = 7125;
    char     apiKey[65]      = {0};
    bool     ssl             = false;
    uint32_t pollIntervalMs  = 1000;

    bool configured() const { return host[0] != '\0'; }
};

// =============================================================================
// Общие настройки (active integration)
// =============================================================================

struct CommonConfig
{
    ActiveIntegration active = ActiveIntegration::None;
};

// =============================================================================
// Runtime state для публикации в `integrations/status`
// =============================================================================

// =============================================================================
// Bambu: payload одной apply-команды (`commands/bambu_apply`)
// =============================================================================

/// Спец-значения `amsId` / `trayId` в `commands/bambu_apply`: если поле `null`
/// в JSON, LINK подставляет default из `BambuConfig` (255/254).
constexpr uint8_t kBambuApplyAmsFromConfig  = 0xFF;
constexpr uint8_t kBambuApplyTrayFromConfig = 0xFE;

/// Данные одного применения филамента в Bambu AMS.
/// Собирается парсером из `commands/bambu_apply`. Поля 1-в-1
/// соответствуют Bambu MQTT `ams_filament_setting`.
struct BambuApplyPayload
{
    uint8_t  amsId          = kBambuApplyAmsFromConfig;
    uint8_t  trayId         = kBambuApplyTrayFromConfig;
    char     trayType[16]   = {0};  // "PLA", "PETG", "ABS", ...
    char     colorHex[10]   = {0};  // RGBA 8 hex + '\0'; 6 hex → дополняется "FF"
    uint16_t nozzleTempMin  = 0;
    uint16_t nozzleTempMax  = 0;
    char     trayInfoIdx[8] = {0};  // "GFL99", "GFA00", ...
    char     settingId[32]  = {0};  // обычно пусто
    char     spoolId[40]    = {0};  // UUID, для диагностики
    char     uid[32]        = {0};  // UID RFID-метки, для диагностики

    bool valid() const
    {
        return trayType[0] != '\0' && colorHex[0] != '\0'
            && nozzleTempMin > 0 && nozzleTempMax > 0
            && trayInfoIdx[0] != '\0';
    }
};

// =============================================================================
// Runtime state для публикации в `integrations/status`
// =============================================================================

/// Унифицированные состояния каждой секции (общие для всех трёх).
enum class IntegrationState : uint8_t
{
    Disabled,       // active != эта интеграция
    Idle,           // активна, настроена, но ещё не соединилась
    Connecting,     // идёт попытка подключения
    Online,         // соединение установлено
    ConfigMissing,  // активна, но параметры пусты/невалидны
    Error,          // ошибка, см. lastError
};

const char* integrationStateToString(IntegrationState value);

} // namespace cloud
} // namespace idryer
