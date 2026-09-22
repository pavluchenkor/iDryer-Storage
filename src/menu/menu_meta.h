// Auto-generated for ESP32 LINK. Do not edit.
// Contains menu metadata only (no pointers to data or callbacks).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MENU_META_COUNT 60
#define MENU_LANG_COUNT 2
#define MENU_SERIALIZED_MAX_SIZE 5626

typedef enum {
    META_SUBMENU = 0,
    META_ACTION = 1,
    META_VALUE = 2,
    META_TOGGLE = 3
} MenuMetaType;

typedef enum {
    META_VT_F32 = 0,
    META_VT_U16 = 1,
    META_VT_U8 = 2,
    META_VT_I32 = 3,
    META_VT_BOOL = 4,
    META_VT_U32 = 5
} MenuMetaValueType;

typedef enum {
    META_SCOPE_GLOBAL = 0,
    META_SCOPE_PER_UNIT = 1
} MenuMetaScope;

typedef struct {
    uint16_t id;
    const char* title[MENU_LANG_COUNT];
    const char* unit[MENU_LANG_COUNT];
    MenuMetaType type;
    int16_t parent;
    int16_t first_child;
    uint16_t child_count;
    MenuMetaValueType vtype;
    float min_val;
    float max_val;
    float step;
    MenuMetaScope scope;
    // menu_protocol_v1: канонические роли и хардкод-виджеты для портала.
    // role — стабильное имя из canonical_roles в mqtt_contract.yaml.
    // widget — override дефолтного UI-компонента (ProfileEditor / RfidWriter / LedPulse).
    // Оба nullptr для приватных пунктов меню (не публикуются на портал).
    const char* role;
    const char* widget;
} MenuMeta;

static const MenuMeta g_menu_meta[MENU_META_COUNT] = {
    // [0] root
    { 0, { "STORAGE LINK", "STORAGE LINK" }, { nullptr, nullptr },
      META_SUBMENU, -1, 1, 7,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [1] led_strip
    { 1, { "ЛЕНТА", "LED STRIP" }, { nullptr, nullptr },
      META_SUBMENU, 0, 2, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [2] led_count
    { 2, { "КОЛ-ВО СВЕТОДИОДОВ", "LED COUNT" }, { "шт", "pcs" },
      META_VALUE, 1, -1, 0,
      META_VT_U16, 1.0f, 300.0f, 1.0f, META_SCOPE_GLOBAL,
      "storage.led_count", nullptr },
    // [3] psu_ma
    { 3, { "ТОК БП", "PSU CURRENT" }, { "мА", "mA" },
      META_VALUE, 1, -1, 0,
      META_VT_U16, 500.0f, 20000.0f, 100.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [4] brightness
    { 4, { "ЯРКОСТЬ", "BRIGHTNESS" }, { "", "" },
      META_VALUE, 1, -1, 0,
      META_VT_U8, 0.0f, 255.0f, 5.0f, META_SCOPE_GLOBAL,
      "storage.led_brightness", nullptr },
    // [5] chipset_group
    { 5, { "ТИП ЛЕНТЫ", "CHIPSET" }, { nullptr, nullptr },
      META_SUBMENU, 1, 6, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [6] chip_ws2812b
    { 6, { "WS2812B", "WS2812B" }, { nullptr, nullptr },
      META_TOGGLE, 5, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [7] chip_ws2811
    { 7, { "WS2811", "WS2811" }, { nullptr, nullptr },
      META_TOGGLE, 5, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [8] chip_ws2813
    { 8, { "WS2813", "WS2813" }, { nullptr, nullptr },
      META_TOGGLE, 5, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [9] chip_ws2815
    { 9, { "WS2815", "WS2815" }, { nullptr, nullptr },
      META_TOGGLE, 5, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [10] chip_sk6812
    { 10, { "SK6812", "SK6812" }, { nullptr, nullptr },
      META_TOGGLE, 5, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [11] color_order_group
    { 11, { "ПОРЯДОК ЦВЕТОВ", "COLOR ORDER" }, { nullptr, nullptr },
      META_SUBMENU, 1, 12, 6,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [12] order_rgb
    { 12, { "RGB", "RGB" }, { nullptr, nullptr },
      META_TOGGLE, 11, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [13] order_rbg
    { 13, { "RBG", "RBG" }, { nullptr, nullptr },
      META_TOGGLE, 11, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [14] order_grb
    { 14, { "GRB", "GRB" }, { nullptr, nullptr },
      META_TOGGLE, 11, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [15] order_gbr
    { 15, { "GBR", "GBR" }, { nullptr, nullptr },
      META_TOGGLE, 11, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [16] order_brg
    { 16, { "BRG", "BRG" }, { nullptr, nullptr },
      META_TOGGLE, 11, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [17] order_bgr
    { 17, { "BGR", "BGR" }, { nullptr, nullptr },
      META_TOGGLE, 11, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [18] idle_lighting
    { 18, { "ФОН", "IDLE LIGHTING" }, { nullptr, nullptr },
      META_SUBMENU, 0, 19, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [19] idle_enabled
    { 19, { "ВКЛЮЧЕНА", "ENABLED" }, { nullptr, nullptr },
      META_TOGGLE, 18, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [20] anim_group
    { 20, { "АНИМАЦИЯ", "ANIMATION" }, { nullptr, nullptr },
      META_SUBMENU, 18, 21, 15,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [21] anim_solid
    { 21, { "Цвет", "Solid" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [22] anim_breathe
    { 22, { "Дыхание", "Breathe" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [23] anim_wave
    { 23, { "Волна", "Wave" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [24] anim_rainbow
    { 24, { "Радуга", "Rainbow" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [25] anim_twinkle
    { 25, { "Мерцание", "Twinkle" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [26] anim_cycle
    { 26, { "Переливы", "Color cycle" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [27] anim_aurora
    { 27, { "Сияние", "Aurora" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [28] anim_candle
    { 28, { "Свеча", "Candle" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [29] anim_ocean
    { 29, { "Океан", "Ocean" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [30] anim_lava
    { 30, { "Лава", "Lava" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [31] anim_forest
    { 31, { "Лес", "Forest" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [32] anim_swell
    { 32, { "Прилив", "Swell" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [33] anim_ripple
    { 33, { "Рябь", "Ripple" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [34] anim_spotlight
    { 34, { "Прожектор", "Spotlight" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [35] anim_duo
    { 35, { "Два цвета", "Two tones" }, { nullptr, nullptr },
      META_TOGGLE, 20, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [36] idle_color_group
    { 36, { "ЦВЕТ", "COLOR" }, { nullptr, nullptr },
      META_SUBMENU, 18, 37, 8,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [37] idle_red
    { 37, { "Красный", "Red" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [38] idle_orange
    { 38, { "Оранжевый", "Orange" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [39] idle_yellow
    { 39, { "Жёлтый", "Yellow" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [40] idle_green
    { 40, { "Зелёный", "Green" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [41] idle_cyan
    { 41, { "Голубой", "Cyan" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [42] idle_blue
    { 42, { "Синий", "Blue" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [43] idle_magenta
    { 43, { "Фиолетовый", "Magenta" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [44] idle_white
    { 44, { "Белый", "White" }, { nullptr, nullptr },
      META_TOGGLE, 36, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [45] pulse_defaults
    { 45, { "ИМПУЛЬС", "PULSE DEFAULTS" }, { nullptr, nullptr },
      META_SUBMENU, 0, 46, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [46] pulse_color_group
    { 46, { "ЦВЕТ", "COLOR" }, { nullptr, nullptr },
      META_SUBMENU, 45, 47, 8,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [47] pulse_red
    { 47, { "Красный", "Red" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [48] pulse_orange
    { 48, { "Оранжевый", "Orange" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [49] pulse_yellow
    { 49, { "Жёлтый", "Yellow" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [50] pulse_green
    { 50, { "Зелёный", "Green" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [51] pulse_cyan
    { 51, { "Голубой", "Cyan" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [52] pulse_blue
    { 52, { "Синий", "Blue" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [53] pulse_magenta
    { 53, { "Фиолетовый", "Magenta" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [54] pulse_white
    { 54, { "Белый", "White" }, { nullptr, nullptr },
      META_TOGGLE, 46, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [55] pulse_duration
    { 55, { "ДЛИТЕЛЬНОСТЬ", "DURATION" }, { "сек", "sec" },
      META_VALUE, 45, -1, 0,
      META_VT_U16, 1.0f, 600.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [56] led_pulse
    { 56, { "ИМПУЛЬС", "LED PULSE" }, { nullptr, nullptr },
      META_ACTION, 0, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      "led.pulse", "LedPulse" },
    // [57] ignore_external_cmd
    { 57, { "ИГНОР. ВНЕШ. КОМАНД", "IGNOR EXT CMD" }, { nullptr, nullptr },
      META_TOGGLE, 0, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      "system.ignore_external_cmd", nullptr },
    // [58] units_count
    { 58, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 1.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [59] language
    { 59, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 0.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
};

static inline const MenuMeta* menu_meta_get(uint16_t id) {
    if (id < MENU_META_COUNT) return &g_menu_meta[id];
    return nullptr;
}
