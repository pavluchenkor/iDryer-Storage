// Auto-generated for ESP32 LINK. Do not edit.
// Contains menu metadata only (no pointers to data or callbacks).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MENU_META_COUNT 49
#define MENU_LANG_COUNT 2

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
      META_SUBMENU, 18, 21, 4,
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
    // [25] idle_color_group
    { 25, { "ЦВЕТ", "COLOR" }, { nullptr, nullptr },
      META_SUBMENU, 18, 26, 8,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [26] idle_red
    { 26, { "Красный", "Red" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [27] idle_orange
    { 27, { "Оранжевый", "Orange" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [28] idle_yellow
    { 28, { "Жёлтый", "Yellow" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [29] idle_green
    { 29, { "Зелёный", "Green" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [30] idle_cyan
    { 30, { "Голубой", "Cyan" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [31] idle_blue
    { 31, { "Синий", "Blue" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [32] idle_magenta
    { 32, { "Фиолетовый", "Magenta" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [33] idle_white
    { 33, { "Белый", "White" }, { nullptr, nullptr },
      META_TOGGLE, 25, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [34] pulse_defaults
    { 34, { "ИМПУЛЬС", "PULSE DEFAULTS" }, { nullptr, nullptr },
      META_SUBMENU, 0, 35, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [35] pulse_color_group
    { 35, { "ЦВЕТ", "COLOR" }, { nullptr, nullptr },
      META_SUBMENU, 34, 36, 8,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      nullptr, nullptr },
    // [36] pulse_red
    { 36, { "Красный", "Red" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [37] pulse_orange
    { 37, { "Оранжевый", "Orange" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [38] pulse_yellow
    { 38, { "Жёлтый", "Yellow" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [39] pulse_green
    { 39, { "Зелёный", "Green" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [40] pulse_cyan
    { 40, { "Голубой", "Cyan" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [41] pulse_blue
    { 41, { "Синий", "Blue" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [42] pulse_magenta
    { 42, { "Фиолетовый", "Magenta" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [43] pulse_white
    { 43, { "Белый", "White" }, { nullptr, nullptr },
      META_TOGGLE, 35, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [44] pulse_duration
    { 44, { "ДЛИТЕЛЬНОСТЬ", "DURATION" }, { "сек", "sec" },
      META_VALUE, 34, -1, 0,
      META_VT_U16, 1.0f, 600.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [45] led_pulse
    { 45, { "ИМПУЛЬС", "LED PULSE" }, { nullptr, nullptr },
      META_ACTION, 0, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT,
      "led.pulse", "LedPulse" },
    // [46] ignore_external_cmd
    { 46, { "ИГНОР. ВНЕШ. КОМАНД", "IGNOR EXT CMD" }, { nullptr, nullptr },
      META_TOGGLE, 0, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL,
      "system.ignore_external_cmd", nullptr },
    // [47] units_count
    { 47, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 1.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
    // [48] language
    { 48, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 0.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL,
      nullptr, nullptr },
};

static inline const MenuMeta* menu_meta_get(uint16_t id) {
    if (id < MENU_META_COUNT) return &g_menu_meta[id];
    return nullptr;
}
