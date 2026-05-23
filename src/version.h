#include <Arduino.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 2

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define VERSION_STR STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH)