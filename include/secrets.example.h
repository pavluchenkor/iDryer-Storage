#pragma once

// Локальный файл (в .gitignore). Опционально хардкодит WiFi-креды
// в прошивку, чтобы не проходить Improv-провизионинг при отладке.
// Оставьте макросы закомментированными — будет использоваться Improv.

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

// MQTT_BROKER / MQTT_PORT / MQTT_USE_TLS задаются через build-флаги
// в platformio.ini для каждого окружения. Здесь не определяются.
