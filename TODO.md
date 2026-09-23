# TODO

English is the main version. Russian: [TODO.ru.md](TODO.ru.md).

- **Print progress on the LED strip.**
  Show the progress of the printer the cabinet is bound to as a bar on the strip. The
  data comes from Bambu or Moonraker, so one of them has to be compiled back in
  (`IDRYER_WITH_BAMBU=1` or `IDRYER_WITH_MOONRAKER=1` in `platformio.ini`). The cost is
  16.5 KB and 22 KB of flash respectively, 36 KB for both — with the new partition
  layout there are about 760 KB free, so either fits easily. The cabinet also needs its
  own printer binding in the portal.

- **The five noise animations are commented out, not deleted.**
  Aurora, candle, ocean, lava and forest were switched off when flash was tight on the
  old 1280 KB partition. There is room again. Decide whether to bring them back or drop
  them for good; the code sits commented out next to the remaining effects in
  `led_strip_animations.cpp`, `led_strip_menu.cpp` and `menu.yaml`.
