/**
 * @file idryer_protocol.h
 * @brief Публичный агрегирующий include библиотеки.
 */

#pragma once

// UART
#include "uart/uart_protocol.h"
#include "uart/uart_bridge.h"

// MQTT
#include "mqtt/idryer_topics.h"
#include "mqtt/mqtt_client.h"

// Cloud
#include "cloud/cloud_state_machine.h"
#include "cloud/telemetry_publisher.h"
#include "cloud/command_handler.h"
#include "cloud/http_api.h"
#include "config/config_manager.h"

// Platform interfaces
#include "device/interfaces/DeviceIdentity.h"
#include "device/interfaces/IWifiManager.h"
#include "device/interfaces/IHttpClient.h"
#include "device/interfaces/ICredentialStore.h"

// Версия библиотеки
#define IDRYER_PROTOCOL_VERSION "0.3.0"
#define IDRYER_PROTOCOL_VERSION_MAJOR 0
#define IDRYER_PROTOCOL_VERSION_MINOR 3
#define IDRYER_PROTOCOL_VERSION_PATCH 0
