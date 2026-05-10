// Auto-generated. Do not edit.
#include <stddef.h>
#include "menu_ids.h"
#include "menu_types.h"
#include "menu_state.h"

#ifdef __cplusplus
extern "C" {
#endif
void led_pulse(void);
#ifdef __cplusplus
}
#endif

extern MenuState menu;

const MenuItem g_menu[MENU__COUNT] = {
  [0] = {
    MENU_ROOT, { "STORAGE LINK", "STORAGE LINK" }, { nullptr, nullptr },
    MN_SUBMENU, -1, 1, 6,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [1] = {
    MENU_LED_STRIP, { "ЛЕНТА", "LED STRIP" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 2, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [2] = {
    MENU_LED_COUNT, { "КОЛ-ВО СВЕТОДИОДОВ", "LED COUNT" }, { "шт", "pcs" },
    MN_VALUE, 1, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.led_count, 1, 300, 1, nullptr, false } },
    -1, 0
  },
  [3] = {
    MENU_PSU_MA, { "ТОК БП", "PSU CURRENT" }, { "мА", "mA" },
    MN_VALUE, 1, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.psu_ma, 500, 20000, 100, nullptr, false } },
    -1, 0
  },
  [4] = {
    MENU_BRIGHTNESS, { "ЯРКОСТЬ", "BRIGHTNESS" }, { "", "" },
    MN_VALUE, 1, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.brightness, 0, 255, 5, nullptr, false } },
    -1, 0
  },
  [5] = {
    MENU_CHIPSET_GROUP, { "ТИП ЛЕНТЫ", "CHIPSET" }, { nullptr, nullptr },
    MN_SUBMENU, 1, 6, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [6] = {
    MENU_CHIP_WS2812B, { "WS2812B", "WS2812B" }, { nullptr, nullptr },
    MN_TOGGLE, 5, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.chip_ws2812b, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [7] = {
    MENU_CHIP_WS2811, { "WS2811", "WS2811" }, { nullptr, nullptr },
    MN_TOGGLE, 5, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.chip_ws2811, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [8] = {
    MENU_CHIP_WS2813, { "WS2813", "WS2813" }, { nullptr, nullptr },
    MN_TOGGLE, 5, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.chip_ws2813, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [9] = {
    MENU_CHIP_WS2815, { "WS2815", "WS2815" }, { nullptr, nullptr },
    MN_TOGGLE, 5, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.chip_ws2815, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [10] = {
    MENU_CHIP_SK6812, { "SK6812", "SK6812" }, { nullptr, nullptr },
    MN_TOGGLE, 5, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.chip_sk6812, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [11] = {
    MENU_COLOR_ORDER_GROUP, { "ПОРЯДОК ЦВЕТОВ", "COLOR ORDER" }, { nullptr, nullptr },
    MN_SUBMENU, 1, 12, 6,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [12] = {
    MENU_ORDER_RGB, { "RGB", "RGB" }, { nullptr, nullptr },
    MN_TOGGLE, 11, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.order_rgb, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [13] = {
    MENU_ORDER_RBG, { "RBG", "RBG" }, { nullptr, nullptr },
    MN_TOGGLE, 11, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.order_rbg, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [14] = {
    MENU_ORDER_GRB, { "GRB", "GRB" }, { nullptr, nullptr },
    MN_TOGGLE, 11, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.order_grb, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [15] = {
    MENU_ORDER_GBR, { "GBR", "GBR" }, { nullptr, nullptr },
    MN_TOGGLE, 11, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.order_gbr, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [16] = {
    MENU_ORDER_BRG, { "BRG", "BRG" }, { nullptr, nullptr },
    MN_TOGGLE, 11, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.order_brg, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [17] = {
    MENU_ORDER_BGR, { "BGR", "BGR" }, { nullptr, nullptr },
    MN_TOGGLE, 11, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.order_bgr, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [18] = {
    MENU_IDLE_LIGHTING, { "ФОН", "IDLE LIGHTING" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 19, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [19] = {
    MENU_IDLE_ENABLED, { "ВКЛЮЧЕНА", "ENABLED" }, { nullptr, nullptr },
    MN_TOGGLE, 18, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_enabled, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [20] = {
    MENU_ANIM_GROUP, { "АНИМАЦИЯ", "ANIMATION" }, { nullptr, nullptr },
    MN_SUBMENU, 18, 21, 4,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [21] = {
    MENU_ANIM_SOLID, { "Цвет", "Solid" }, { nullptr, nullptr },
    MN_TOGGLE, 20, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.anim_solid, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [22] = {
    MENU_ANIM_BREATHE, { "Дыхание", "Breathe" }, { nullptr, nullptr },
    MN_TOGGLE, 20, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.anim_breathe, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [23] = {
    MENU_ANIM_WAVE, { "Волна", "Wave" }, { nullptr, nullptr },
    MN_TOGGLE, 20, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.anim_wave, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [24] = {
    MENU_ANIM_RAINBOW, { "Радуга", "Rainbow" }, { nullptr, nullptr },
    MN_TOGGLE, 20, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.anim_rainbow, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [25] = {
    MENU_IDLE_COLOR_GROUP, { "ЦВЕТ", "COLOR" }, { nullptr, nullptr },
    MN_SUBMENU, 18, 26, 8,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [26] = {
    MENU_IDLE_RED, { "Красный", "Red" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_red, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [27] = {
    MENU_IDLE_ORANGE, { "Оранжевый", "Orange" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_orange, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [28] = {
    MENU_IDLE_YELLOW, { "Жёлтый", "Yellow" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_yellow, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [29] = {
    MENU_IDLE_GREEN, { "Зелёный", "Green" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_green, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [30] = {
    MENU_IDLE_CYAN, { "Голубой", "Cyan" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_cyan, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [31] = {
    MENU_IDLE_BLUE, { "Синий", "Blue" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_blue, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [32] = {
    MENU_IDLE_MAGENTA, { "Фиолетовый", "Magenta" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_magenta, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [33] = {
    MENU_IDLE_WHITE, { "Белый", "White" }, { nullptr, nullptr },
    MN_TOGGLE, 25, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.idle_white, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [34] = {
    MENU_PULSE_DEFAULTS, { "ИМПУЛЬС", "PULSE DEFAULTS" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 35, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [35] = {
    MENU_PULSE_COLOR_GROUP, { "ЦВЕТ", "COLOR" }, { nullptr, nullptr },
    MN_SUBMENU, 34, 36, 8,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [36] = {
    MENU_PULSE_RED, { "Красный", "Red" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_red, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [37] = {
    MENU_PULSE_ORANGE, { "Оранжевый", "Orange" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_orange, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [38] = {
    MENU_PULSE_YELLOW, { "Жёлтый", "Yellow" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_yellow, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [39] = {
    MENU_PULSE_GREEN, { "Зелёный", "Green" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_green, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [40] = {
    MENU_PULSE_CYAN, { "Голубой", "Cyan" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_cyan, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [41] = {
    MENU_PULSE_BLUE, { "Синий", "Blue" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_blue, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [42] = {
    MENU_PULSE_MAGENTA, { "Фиолетовый", "Magenta" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_magenta, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [43] = {
    MENU_PULSE_WHITE, { "Белый", "White" }, { nullptr, nullptr },
    MN_TOGGLE, 35, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.pulse_white, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [44] = {
    MENU_PULSE_DURATION, { "ДЛИТЕЛЬНОСТЬ", "DURATION" }, { "сек", "sec" },
    MN_VALUE, 34, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.pulse_dur_sec, 1, 600, 1, nullptr, false } },
    -1, 0
  },
  [45] = {
    MENU_LED_PULSE, { "ИМПУЛЬС", "LED PULSE" }, { nullptr, nullptr },
    MN_ACTION, 0, -1, 0,
    { { led_pulse }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [46] = {
    MENU_UNITS_COUNT, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
    MN_VALUE, 0, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.units_count, 1, 1, 1, nullptr, false } },
    -1, 0
  },
  [47] = {
    MENU_LANGUAGE, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
    MN_VALUE, 0, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.language, 0, 1, 1, nullptr, false } },
    -1, 0
  },
};
