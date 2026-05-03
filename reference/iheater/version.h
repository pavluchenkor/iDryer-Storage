#pragma once

// VERSION_MAJOR при желании может быть взят из lib/idryer-menu/src/version.h,
// если такой файл присутствует в кэше menu. Иначе используется локальный fallback.
// VERSION_MINOR и VERSION_PATCH задаются на стороне LINK.

#include <Arduino.h>

// Если в menu cache есть version.h, используем его для VERSION_MAJOR
#if __has_include("../lib/idryer-menu/src/version.h")
#include "../lib/idryer-menu/src/version.h"
#undef VERSION_MINOR
#undef VERSION_PATCH
#undef VERSION_STR
#undef STR
#undef STR_HELPER
#else
// Fallback если файл еще не скопирован
#define VERSION_MAJOR 1
#endif

// Собственные версии LINK
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define VERSION_STR STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH)

// Дополнительные форматы версии для совместимости
#define VERSION_STRING VERSION_STR
#define VERSION_NUMBER ((VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | VERSION_PATCH)
