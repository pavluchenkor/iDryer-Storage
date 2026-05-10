#if defined(ESP32) || defined(ESP_PLATFORM)

#include "led_strip_menu.h"
#include "led_strip_executor.h"

#include <string.h>

#include <hal/hal_types.h>
#include <menu_bindings.h>
#include <menu_ids.h>
#include <menu_state.h>

// ── Toggle group helpers ────────────────────────────────────────────
// Каждый сеттер выставляет ровно один toggle в группе, остальные сбрасывает.
// Каждое изменение persist'ится через menu_apply_by_bind.

namespace {

constexpr const char* kChipsetBinds[] = {
    "chip_ws2812b", "chip_ws2811", "chip_ws2813", "chip_ws2815", "chip_sk6812",
};
constexpr size_t kChipsetCount = sizeof(kChipsetBinds) / sizeof(kChipsetBinds[0]);

constexpr const char* kOrderBinds[] = {
    "order_rgb", "order_rbg", "order_grb", "order_gbr", "order_brg", "order_bgr",
};
constexpr size_t kOrderCount = sizeof(kOrderBinds) / sizeof(kOrderBinds[0]);

constexpr const char* kAnimBinds[] = {
    "anim_solid", "anim_breathe", "anim_wave", "anim_rainbow",
};
constexpr size_t kAnimCount = sizeof(kAnimBinds) / sizeof(kAnimBinds[0]);

constexpr const char* kIdleColorBinds[] = {
    "idle_red",  "idle_orange",  "idle_yellow",  "idle_green",
    "idle_cyan", "idle_blue",    "idle_magenta", "idle_white",
};
constexpr size_t kIdleColorCount = sizeof(kIdleColorBinds) / sizeof(kIdleColorBinds[0]);

constexpr const char* kPulseColorBinds[] = {
    "pulse_red",  "pulse_orange",  "pulse_yellow",  "pulse_green",
    "pulse_cyan", "pulse_blue",    "pulse_magenta", "pulse_white",
};
constexpr size_t kPulseColorCount = sizeof(kPulseColorBinds) / sizeof(kPulseColorBinds[0]);

// Restored from same indexing as kIdleColorBinds / kPulseColorBinds.
constexpr CRGB kPaletteRGB[8] = {
    CRGB(0xFF, 0x00, 0x00),  // 0 Red
    CRGB(0xFF, 0x60, 0x00),  // 1 Orange
    CRGB(0xFF, 0xC0, 0x00),  // 2 Yellow (тёплый, не зелёный)
    CRGB(0x00, 0xFF, 0x00),  // 3 Green
    CRGB(0x00, 0xC0, 0xFF),  // 4 Cyan (чуть синее, чтобы выглядел голубым)
    CRGB(0x00, 0x00, 0xFF),  // 5 Blue
    CRGB(0xFF, 0x00, 0xFF),  // 6 Magenta
    CRGB(0xFF, 0xFF, 0xFF),  // 7 White
};

// Найти индекс активного toggle в группе. Возвращает defaultIdx если ни один.
uint8_t pickActiveIndex(const char* const* binds, size_t count, uint8_t defaultIdx) {
    for (size_t i = 0; i < count; i++) {
        bool v = false;
        if (menu_read_by_bind(binds[i], &v) && v) return (uint8_t)i;
    }
    return defaultIdx;
}

// Активировать toggle keepIdx, сбросить остальные в группе.
// onlyIfActivating == true — никаких записей если keepIdx уже активен и остальные false.
void activateExclusive(const char* const* binds, size_t count, uint8_t keepIdx) {
    for (size_t i = 0; i < count; i++) {
        menu_apply_by_bind(binds[i], (i == keepIdx) ? 1.0f : 0.0f);
    }
}

// Если в группе 0 или ≥2 активных — оставить firstIdx как канонический.
void normalizeGroup(const char* const* binds, size_t count, uint8_t firstIdx, const char* groupName) {
    int activeCount = 0;
    int firstFound = -1;
    for (size_t i = 0; i < count; i++) {
        bool v = false;
        if (menu_read_by_bind(binds[i], &v) && v) {
            if (firstFound < 0) firstFound = (int)i;
            activeCount++;
        }
    }
    if (activeCount == 1) return;
    uint8_t keep = (firstFound >= 0) ? (uint8_t)firstFound : firstIdx;
    activateExclusive(binds, count, keep);
    HAL_LOG_WARN("MENU", "%s group: %d active, kept %s",
                 groupName, activeCount, binds[keep]);
}

// Проверить — это id из toggle-группы? Если да — активировать exclusive.
// Возвращает true если id обработан.
bool tryHandleGroupActivation(int id, int val) {
    // Группа chipset
    static const int chipsetIds[]    = {MENU_CHIP_WS2812B, MENU_CHIP_WS2811, MENU_CHIP_WS2813, MENU_CHIP_WS2815, MENU_CHIP_SK6812};
    // Группа order
    static const int orderIds[]      = {MENU_ORDER_RGB, MENU_ORDER_RBG, MENU_ORDER_GRB, MENU_ORDER_GBR, MENU_ORDER_BRG, MENU_ORDER_BGR};
    // Группа anim
    static const int animIds[]       = {MENU_ANIM_SOLID, MENU_ANIM_BREATHE, MENU_ANIM_WAVE, MENU_ANIM_RAINBOW};
    // Группа idle color
    static const int idleColorIds[]  = {MENU_IDLE_RED, MENU_IDLE_ORANGE, MENU_IDLE_YELLOW, MENU_IDLE_GREEN,
                                         MENU_IDLE_CYAN, MENU_IDLE_BLUE, MENU_IDLE_MAGENTA, MENU_IDLE_WHITE};
    // Группа pulse color
    static const int pulseColorIds[] = {MENU_PULSE_RED, MENU_PULSE_ORANGE, MENU_PULSE_YELLOW, MENU_PULSE_GREEN,
                                         MENU_PULSE_CYAN, MENU_PULSE_BLUE, MENU_PULSE_MAGENTA, MENU_PULSE_WHITE};

    auto handle = [&](const int* ids, const char* const* binds, size_t count, const char* groupName) -> int {
        for (size_t i = 0; i < count; i++) {
            if (ids[i] == id) {
                if (val == 1) {
                    activateExclusive(binds, count, (uint8_t)i);
                    HAL_LOG_INFO("MENU", "%s = %s", groupName, binds[i]);
                }
                // val==0 для toggle-группы игнорируется — нельзя оставить ноль активных.
                return 1;
            }
        }
        return 0;
    };

    if (handle(chipsetIds,    kChipsetBinds,    kChipsetCount,    "chipset"))    return true;
    if (handle(orderIds,      kOrderBinds,      kOrderCount,      "order"))      return true;
    if (handle(animIds,       kAnimBinds,       kAnimCount,       "anim"))       return true;
    if (handle(idleColorIds,  kIdleColorBinds,  kIdleColorCount,  "idle_color")) return true;
    if (handle(pulseColorIds, kPulseColorBinds, kPulseColorCount, "pulse_color"))return true;
    return false;
}

} // namespace

// ── Public API ──────────────────────────────────────────────────────

uint8_t selectedChipset() {
    return pickActiveIndex(kChipsetBinds, kChipsetCount, 0);  // default WS2812B
}

uint8_t selectedColorOrder() {
    return pickActiveIndex(kOrderBinds, kOrderCount, 2);  // default GRB
}

uint8_t selectedAnimation() {
    return pickActiveIndex(kAnimBinds, kAnimCount, 0);  // default Solid
}

CRGB selectedIdleColor() {
    return kPaletteRGB[pickActiveIndex(kIdleColorBinds, kIdleColorCount, 7)];  // default White
}

CRGB selectedPulseColor() {
    return kPaletteRGB[pickActiveIndex(kPulseColorBinds, kPulseColorCount, 7)];  // default White
}

uint8_t selectedBrightness() {
    return menu.brightness;
}

bool isIdleEnabled() {
    return menu.idle_enabled;
}

uint16_t pulseDefaultDurationSec() {
    return menu.pulse_dur_sec;
}

void normalizeMenuGroups() {
    normalizeGroup(kChipsetBinds,    kChipsetCount,    0, "chipset");
    normalizeGroup(kOrderBinds,      kOrderCount,      2, "order");      // GRB канон
    normalizeGroup(kAnimBinds,       kAnimCount,       0, "anim");       // Solid канон
    normalizeGroup(kIdleColorBinds,  kIdleColorCount,  7, "idle_color"); // White канон
    normalizeGroup(kPulseColorBinds, kPulseColorCount, 7, "pulse_color");// White канон
}

void applyMenuToExecutor(LedStripExecutor& exec) {
    exec.setLedsCount(menu.led_count);
    exec.setMaxCurrentMa(menu.psu_ma);
    exec.setBrightness(menu.brightness);
    HAL_LOG_INFO("MENU", "applied: count=%u psu=%u mA brightness=%u chip=%u order=%u",
                 menu.led_count, menu.psu_ma, menu.brightness,
                 selectedChipset(), selectedColorOrder());
}

bool applyConfigChange(int id, int val, LedStripExecutor& exec, OnMenuChanged onChanged) {
    bool changed = false;

    switch (id) {
        case MENU_LED_COUNT:
            if (val < 1 || val > 300) {
                HAL_LOG_WARN("MENU", "led_count out of range: %d", val);
                return false;
            }
            menu_apply_by_bind("led_count", (float)val);
            exec.setLedsCount(menu.led_count);
            changed = true;
            break;

        case MENU_PSU_MA:
            if (val < 500 || val > 20000) {
                HAL_LOG_WARN("MENU", "psu_ma out of range: %d", val);
                return false;
            }
            menu_apply_by_bind("psu_ma", (float)val);
            exec.setMaxCurrentMa(menu.psu_ma);
            changed = true;
            break;

        case MENU_BRIGHTNESS:
            if (val < 0 || val > 255) {
                HAL_LOG_WARN("MENU", "brightness out of range: %d", val);
                return false;
            }
            menu_apply_by_bind("brightness", (float)val);
            exec.setBrightness(menu.brightness);
            changed = true;
            break;

        case MENU_IDLE_ENABLED:
            menu_apply_by_bind("idle_enabled", val ? 1.0f : 0.0f);
            HAL_LOG_INFO("MENU", "idle = %s", val ? "ON" : "OFF");
            changed = true;
            break;

        case MENU_PULSE_DURATION:
            if (val < 1 || val > 600) {
                HAL_LOG_WARN("MENU", "pulse_duration out of range: %d", val);
                return false;
            }
            menu_apply_by_bind("pulse_dur_sec", (float)val);
            changed = true;
            break;

        default:
            // Toggle-группы: chipset / order / anim / idle_color / pulse_color.
            if (tryHandleGroupActivation(id, val)) {
                changed = true;
                break;
            }
            HAL_LOG_WARN("MENU", "unknown config id: %d", id);
            return false;
    }

    if (changed && onChanged) onChanged();
    return changed;
}

#endif // ESP32 || ESP_PLATFORM
