/**
 * @file idryer_arduino.h
 * @brief Агрегирует Arduino-реализации платформенных интерфейсов.
 */

#pragma once

// HAL (время/логирование)
#include "../../hal/hal_arduino.h"

// Arduino-реализации интерфейсов
#include "ArduinoWifiManager.h"
#include "ArduinoHttpClient.h"
#include "ArduinoCredentialStore.h"
