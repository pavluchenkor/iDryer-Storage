/**
 * @file HeaterDevice.cpp
 * @brief Реализация фасада iHeater Link — без UART.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "iheater/HeaterDevice.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <Arduino.h>
#include <WiFi.h>

#include <idryer_protocol.h>
#include <hal/hal_types.h>

#include <ctype.h>

#include <menu_state.h>

#include "version.h"

using namespace DryerUart;
using namespace idryer;
using namespace idryer::hal;

namespace iheaterlink
{
    namespace
    {
        /// MAC ESP32 → "DEVICE_aabbccddeeff" (префикс + 12 lowercase hex).
        /// Формат валидирует backend /devices/provision: "DEVICE_<mac>" для ESP32 Link.
        void readEspMacSerial(char *out, size_t outSize)
        {
            if (!out || outSize < 20)
                return;
            uint8_t mac[6] = {0};
            WiFi.macAddress(mac);
            snprintf(out, outSize, "DEVICE_%02x%02x%02x%02x%02x%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        /// Сопоставить название материала от принтера (Bambu/AMS) с температурой
        /// из пользовательского меню iHeater Link.
        ///
        /// Принтер шлёт `trayType` вида "PLA", "PETG", "ABS", "ASA", "PC",
        /// "PA-CF", "PAHT", "Nylon" и т.п. Делаем case-insensitive prefix match
        /// против известных групп. Неизвестный материал → 0 (OFF): греть
        /// «вслепую» небезопасно.
        float materialTempFromMenu(const char *trayType)
        {
            if (!trayType || trayType[0] == '\0')
                return 0.0f;

            char upper[16];
            size_t i = 0;
            for (; trayType[i] && i < sizeof(upper) - 1; i++)
                upper[i] = (char)toupper((unsigned char)trayType[i]);
            upper[i] = '\0';

            if (strncmp(upper, "PETG", 4) == 0)
                return menu.mat_petg;
            if (strncmp(upper, "PLA", 3) == 0)
                return menu.mat_pla;
            if (strncmp(upper, "ABS", 3) == 0)
                return menu.mat_abs;
            if (strncmp(upper, "ASA", 3) == 0)
                return menu.mat_asa;
            if (strncmp(upper, "PC", 2) == 0)
                return menu.mat_pc;
            // PA, PA-CF, PAHT, NYLON — всё в колонку PA/NYLON.
            if (strncmp(upper, "PA", 2) == 0)
                return menu.mat_pa;
            if (strncmp(upper, "NYLON", 5) == 0)
                return menu.mat_pa;

            // Неизвестный материал — не греем.
            return 0.0f;
        }
    } // namespace

    // =============================================================================
    // КОНСТРУКТОР
    // =============================================================================

    HeaterDevice::HeaterDevice(idryer::IWifiManager *wifi,
                               idryer::IHttpClient *http,
                               idryer::ICredentialStore *store,
                               const char *apiBaseUrl)
        : wifi_(wifi),
          http_(http),
          store_(store),
          api_(http, apiBaseUrl ? apiBaseUrl : IDRYER_API_BASE),
          cloud_(wifi, store, &api_, &mqtt_),
          publisher_(&mqtt_),
          integrationsStore_(),
          integrations_(&mqtt_, &integrationsStore_),
          controllerOutput_(RmtOutputConfig{}),
          linkSink_(&controllerOutput_),
          cmdHandler_(&linkSink_),
          menuBridge_(&mqtt_)
    {
    }

    // =============================================================================
    // ИНИЦИАЛИЗАЦИЯ
    // =============================================================================

    void HeaterDevice::begin()
    {
        HAL_LOG_INFO("HEATER", "Initializing...");

        cloud_.setStateChangeCallback(onCloudStateChange, this);
        cloud_.setClaimPinCallback(onClaimPin, this);
        cloud_.setClaimCompleteCallback(onClaimComplete, this);
        cloud_.setUnclaimedCallback(onUnclaimed, this);

        mqtt_.setCommandCallback([this](const char *cmd, JsonObjectConst data)
                                 { handleMqttCommand(cmd, data); });

        cmdHandler_.setTimeSyncCallback([](const char *ts, void *ctx)
                                        { static_cast<HeaterDevice *>(ctx)->syncTimeFromBackend(ts); },
                                        this);

        // Маршрутизация commands/link_integration и commands/bambu_apply → менеджер.
        cmdHandler_.setLinkIntegrationsManager(&integrations_);

        controllerOutput_.begin();
        if (!controllerOutput_.isEnabled())
        {
            HAL_LOG_WARN("HEATER", "RMT output disabled: IHEATER_TRIGGER_OUTPUT_PIN is not configured");
        }

        // В качестве serialNumber для облака и HA-ClientId используем MAC ESP.
        char mac[24] = {0};
        readEspMacSerial(mac, sizeof(mac));
        if (mac[0] != '\0')
        {
            integrations_.setHaClientId(mac);
        }

        // Тип устройства — iHeater Link (ESP32-мост к STM32 iHeater).
        // Влияет на режим Bambu (Reader) внутри менеджера.
        integrations_.setDeviceType(DeviceType::IHeaterLink);

        // Колбэки из менеджера → локальное применение на pulse output.
        integrations_.setVirtualChamberCallback(
            [this](const cloud::VirtualChamberData &data)
            { onVirtualChamberUpdate(data); });
        integrations_.setBambuPrinterStatusCallback(
            [this](const cloud::BambuPrinterStatus &status)
            { onBambuPrinterStatusUpdate(status); });

        integrationsStore_.begin();
        integrations_.begin();

        // Меню: NVS + дефолты + загрузка сохранённых значений.
        // Вызываем до cloud_.begin() чтобы к моменту первого publishInfoOnce/publishFullConfig
        // MenuState и g_menu_cache уже были согласованы.
        //
        // Выбор активного ПОДКЛЮЧЕНИЯ из меню (bambu_en/moon_en/ha_en) транслируется
        // в LinkIntegrationsManager::setActive — без этого Moonraker/Bambu клиент
        // не запустится даже при `enabled:true` в link_integration.
        menuBridge_.setActiveConnectionCallback([this](ActiveConnection kind)
                                                {
            using AI = idryer::cloud::ActiveIntegration;
            AI target = AI::None;
            switch (kind) {
                case ActiveConnection::Bambu:     target = AI::Bambu; break;
                case ActiveConnection::Moonraker: target = AI::Moonraker; break;
                case ActiveConnection::Ha:        target = AI::Ha; break;
                case ActiveConnection::None:      target = AI::None; break;
            }
            HAL_LOG_INFO("HEATER", "Menu→setActive: %d", (int)target);
            integrations_.setActive(target); });

        menuBridge_.begin();

        // cloud_.begin() загружает NVS — вызываем setMcuSerial ПОСЛЕ,
        // чтобы save(identity_) не перезаписал токен из NVS пустой строкой.
        cloud_.begin();

        if (mac[0] != '\0')
        {
            cloud_.setMcuSerial(mac);
        }

        HAL_LOG_INFO("HEATER", "Initialized, serial=%s", cloud_.getIdentity().serialNumber);
    }

    // =============================================================================
    // ГЛАВНЫЙ ЦИКЛ
    // =============================================================================

    // Запустить тестовый sweep: пройти весь диапазон температур вверх-вниз за 30 секунд.
    void HeaterDevice::startRmtSweep()
    {
        const auto &cfg = controllerOutput_.getConfig();
        const int step = cfg.stepTempC > 0 ? cfg.stepTempC : 1;
        const int count = (cfg.maxTempC - cfg.minTempC) / step + 1;
        sweepStepMs_ = 30000u / (count > 1 ? (uint32_t)count : 1u);

        sweepIdx_ = 0;
        sweepDir_ = 1;
        sweepLastMs_ = millis();
        sweepActive_ = true;
        HAL_LOG_INFO("HEATER", "RMT sweep started: %d steps, %ums/step", count, sweepStepMs_);
    }

    // Остановить sweep и выставить выход в Off.
    void HeaterDevice::stopRmtSweep()
    {
        sweepActive_ = false;
        ControllerOutputCommand off{};
        off.mode = ControllerOutputMode::Off;
        controllerOutput_.apply(off);
        controllerOutput_.forceFrame();
        HAL_LOG_INFO("HEATER", "RMT sweep stopped");
    }

    // Один шаг sweep: применить следующую температуру и переключить направление на границах.
    void HeaterDevice::sweepStep()
    {
        const auto &cfg = controllerOutput_.getConfig();
        const int step = cfg.stepTempC > 0 ? cfg.stepTempC : 1;
        const int count = (cfg.maxTempC - cfg.minTempC) / step + 1;

        const uint32_t now = millis();
        if (now - sweepLastMs_ < sweepStepMs_)
            return;
        sweepLastMs_ = now;

        const float temp = cfg.minTempC + sweepIdx_ * step;

        ControllerOutputCommand cmd{};
        cmd.mode = ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = temp;
        controllerOutput_.apply(cmd);
        controllerOutput_.forceFrame();

        HAL_LOG_INFO("HEATER", "RMT sweep: %.0f°C (code=%u)",
                     temp, controllerOutput_.getLastPulseCode());

        sweepIdx_ += sweepDir_;
        if (sweepIdx_ >= count)
        {
            sweepIdx_ = count - 2;
            sweepDir_ = -1;
        }
        if (sweepIdx_ < 0)
        {
            sweepIdx_ = 1;
            sweepDir_ = 1;
        }
    }

    void HeaterDevice::loop()
    {
        cloud_.loop();
        integrations_.loop();
        linkSink_.tick();

        if (sweepActive_)
        {
            sweepStep();
            return;
        }

        if (cloud_.isOnline())
        {
            publishInfoOnce();
            publishLinkTelemetryIfNeeded();
        }
    }

    // =============================================================================
    // CLAIM
    // =============================================================================

    bool HeaterDevice::requestClaimProcess()
    {
        return cloud_.requestClaim();
    }

    // =============================================================================
    // ОБРАБОТКА MQTT-КОМАНД
    // =============================================================================

    void HeaterDevice::handleMqttCommand(const char *command, JsonObjectConst data)
    {
        // Меню портала: перехватываем до CommandHandler — тот пытается слать на UART-MCU,
        // которого на iHeater Link нет (см. LinkCommandSink::GetConfig / SetConfig).
        if (command && strcmp(command, "get_config") == 0)
        {
            menuBridge_.publishFullConfig();
            return;
        }
        if (command && strcmp(command, "set") == 0)
        {
            menuBridge_.applySetCommand(data);
            return;
        }

        // Все остальные команды (drying/stop/link_integration/bambu_apply/...) идут через
        // CommandHandler. CommandHandler сам маршрутизирует link_integration/bambu_apply
        // в LinkIntegrationsManager, остальное — в LinkCommandSink.
        cmdHandler_.handleMqttCommand(command, data);

        // link_integration с enabled:true — переключить активное подключение в меню.
        // Вызываем ПОСЛЕ cmdHandler_, чтобы новый конфиг (host/port) уже был сохранён
        // до того как setActive запустит клиента.
        if (command && strcmp(command, "link_integration") == 0)
        {
            const char *type = data["type"] | (const char *)nullptr;
            const bool enabled = data["enabled"] | false;
            if (type && enabled)
            {
                const char *bind = nullptr;
                if (strcmp(type, "moonraker") == 0)  bind = "moon_en";
                else if (strcmp(type, "bambu") == 0) bind = "bambu_en";
                else if (strcmp(type, "ha") == 0)    bind = "ha_en";

                if (bind)
                {
                    StaticJsonDocument<32> doc;
                    doc["bind"] = bind;
                    doc["val"] = true;
                    menuBridge_.applySetCommand(doc.as<JsonObjectConst>());
                    HAL_LOG_INFO("HEATER", "link_integration: switched active to %s", type);
                }
            }
        }
    }

    // Внешняя команда (WebSocket/WebSerial) — проксируется в тот же обработчик что и MQTT.
    void HeaterDevice::handleExternalCommand(const char *command, JsonObjectConst data)
    {
        handleMqttCommand(command, data);
    }

    // =============================================================================
    // КОЛБЭКИ ОТ ИНТЕГРАЦИЙ
    // =============================================================================

    void HeaterDevice::onVirtualChamberUpdate(const cloud::VirtualChamberData &data)
    {
        // Пользователь выбирает источник target через меню ПОДКЛЮЧЕНИЯ
        // (Bambu / Moonraker / HA — эксклюзивно). Moonraker активен, только если
        // включён MOONRAKER.
        const bool autoHeat = menu.moon_en;

        // target == 0 ИЛИ объект VIRTUAL_CHAMBER не виден → выключаем нагрев.
        // target > 0 → подаём setpoint на pulse output.
        ControllerOutputCommand cmd{};
        if (!autoHeat || !data.available || data.target <= 0.0f)
        {
            cmd.mode = ControllerOutputMode::Off;
            cmd.targetTempC = 0.0f;
        }
        else
        {
            cmd.mode = ControllerOutputMode::TargetTemperature;
            cmd.targetTempC = data.target;
        }
        controllerOutput_.apply(cmd);

        HAL_LOG_INFO("HEATER",
                     "VIRTUAL_CHAMBER: autoHeat=%d available=%d target=%.1f temp=%.1f hasSensor=%d → output=%s",
                     autoHeat ? 1 : 0,
                     data.available ? 1 : 0,
                     data.target,
                     data.temperature,
                     data.hasSensor ? 1 : 0,
                     cmd.mode == ControllerOutputMode::Off ? "OFF" : "ON");
    }

    void HeaterDevice::onBambuPrinterStatusUpdate(const cloud::BambuPrinterStatus &status)
    {
        // Bambu активен, только если пользователь включил BAMBU в меню ПОДКЛЮЧЕНИЯ.
        const bool autoHeat = menu.bambu_en;

        // Приоритеты:
        //   1. Пользователь выключил авто-нагрев → OFF.
        //   2. Принтер сам знает целевую температуру камеры (X1C с датчиком)
        //      → используем chamberTarget.
        //   3. Принтер не задаёт target, но известен материал (AMS tray) →
        //      берём температуру из пользовательских пресетов (menu.mat_*).
        //   4. Нет ни target, ни trayType → OFF.
        ControllerOutputCommand cmd{};
        float menuTemp = 0.0f;
        const char *source = "off";

        if (!autoHeat)
        {
            cmd.mode = ControllerOutputMode::Off;
            cmd.targetTempC = 0.0f;
            source = "autoHeat=off";
        }
        else if (status.chamberTarget > 0.0f)
        {
            cmd.mode = ControllerOutputMode::TargetTemperature;
            cmd.targetTempC = status.chamberTarget;
            source = "printer";
        }
        else if (status.trayType[0] != '\0')
        {
            menuTemp = materialTempFromMenu(status.trayType);
            if (menuTemp > 0.0f)
            {
                cmd.mode = ControllerOutputMode::TargetTemperature;
                cmd.targetTempC = menuTemp;
                source = "menu";
            }
            else
            {
                cmd.mode = ControllerOutputMode::Off;
                cmd.targetTempC = 0.0f;
                source = "menu=0";
            }
        }
        else
        {
            cmd.mode = ControllerOutputMode::Off;
            cmd.targetTempC = 0.0f;
            source = "no-target";
        }

        controllerOutput_.apply(cmd);

        HAL_LOG_INFO("HEATER",
                     "BAMBU status: autoHeat=%d state=%s chamberTarget=%.1f chamberTemp=%.1f tray=%s menu=%.1f → output=%s (src=%s)",
                     autoHeat ? 1 : 0,
                     status.gcodeState,
                     status.chamberTarget,
                     status.chamberTemp,
                     status.trayType,
                     menuTemp,
                     cmd.mode == ControllerOutputMode::Off ? "OFF" : "ON",
                     source);
    }

    // =============================================================================
    // ПУБЛИКАЦИЯ INFO (один раз после первого online)
    // =============================================================================

    void HeaterDevice::publishInfoOnce()
    {
        if (infoPublished_)
            return;

        UnitConfig units[1] = {};
        units[0].unitId = 0;
        units[0].capabilities = 0;
        memset(units[0].scales, 0xFF, sizeof(units[0].scales));
        memset(units[0].rfid, 0xFF, sizeof(units[0].rfid));

        const char *hwVersion = "LINK-v1";
        const char *fwVersion = VERSION_STR;
        const uint32_t workTimeCounter = millis() / 1000u;

        char mcuSerial[24] = {0};
        readEspMacSerial(mcuSerial, sizeof(mcuSerial));

        const uint8_t deviceType = static_cast<uint8_t>(DeviceType::IHeaterLink);

        if (publisher_.publishInfo(1, units, hwVersion, fwVersion,
                                   workTimeCounter, mcuSerial, deviceType))
        {
            infoPublished_ = true;
            HAL_LOG_INFO("HEATER", "Published info: hw=%s fw=%s serial=%s deviceType=%u",
                         hwVersion, fwVersion, mcuSerial, deviceType);
        }
    }

    // =============================================================================
    // ТЕЛЕМЕТРИЯ LINK (каждые 5 секунд)
    // =============================================================================

    void HeaterDevice::publishLinkTelemetryIfNeeded()
    {
        const uint32_t now = millis();
        if (now - lastTelemetryAt_ < 5000u)
            return;
        lastTelemetryAt_ = now;

        if (!mqtt_.isConnected())
            return;

        // Telemetry = только то, что backend реально читает
        // (telemetry.handler.ts + broadcastTelemetryUpdate):
        //   - deviceType, active
        //   - outputMode, targetTempC (heater intent для WS telemetry:update)
        //   - units[] (графики и история в БД)
        //   - rssi, uptime, timestamp (housekeeping)
        // Snapshot активной интеграции (moonraker.name / bambu.currentFilament /
        // printerState / progress / chamberTarget) живёт в `integrations/status`
        // (retained, обновляется при изменении) — дублировать здесь смысла нет.
        StaticJsonDocument<384> doc;
        doc["deviceType"] = "iheater_link";
        doc["active"] = cloud::activeIntegrationToString(integrations_.getActive());

        // Нужны для источников температуры и heater intent (ниже)
        const cloud::MoonrakerStatus &ms = integrations_.moonrakerStatus();
        const cloud::BambuPrinterStatus &bs = integrations_.bambuPrinterStatus();

        // Выход на нагреватель (намерение ESP — STM32 ничего не возвращает).
        const ControllerOutputCommand out = controllerOutput_.getLastCommand();
        const bool heating = (out.mode == ControllerOutputMode::TargetTemperature);
        doc["outputMode"] = static_cast<int>(out.mode);
        doc["targetTempC"] = out.targetTempC;

        // iDryer-совместимый блок units[] — чтобы portal.telemetry.handler
        // сохранял историю и строил графики штатным путём.
        // temperature: температура камеры от активного источника (moonraker с датчиком > bambu).
        // humidity: 0 — на iHeater Link нет датчика влажности.
        // heaterPower: 0/100 как намерение ESP, реальную мощность STM32 не отдаёт.
        float chamberTemp = 0.0f;
        if (ms.chamberHasSensor && ms.chamberTemperature > 0.0f)
        {
            chamberTemp = ms.chamberTemperature;
        }
        else if (bs.chamberTemp > 0.0f)
        {
            chamberTemp = bs.chamberTemp;
        }
        JsonArray units = doc.createNestedArray("units");
        JsonObject u1 = units.createNestedObject();
        u1["unitId"] = "U1";
        u1["temperature"] = chamberTemp;
        u1["humidity"] = 0;
        u1["heaterPower"] = heating ? 100 : 0;
        u1["fanStatus"] = false;

        doc["rssi"] = WiFi.RSSI();
        doc["uptime"] = millis() / 1000u;

        mqtt_.publishTelemetry(doc);
    }

    // =============================================================================
    // СИНХРОНИЗАЦИЯ ВРЕМЕНИ
    // =============================================================================

    void HeaterDevice::syncTimeFromBackend(const char *timestamp)
    {
        if (!timestamp)
            return;

        struct tm tm = {0};
        char cleanTimestamp[32];
        strncpy(cleanTimestamp, timestamp, sizeof(cleanTimestamp) - 1);
        cleanTimestamp[sizeof(cleanTimestamp) - 1] = '\0';

        char *dot = strchr(cleanTimestamp, '.');
        if (dot)
        {
            char *z = strchr(dot, 'Z');
            if (z)
            {
                *dot = 'Z';
                *(dot + 1) = '\0';
            }
        }

        if (strptime(cleanTimestamp, "%Y-%m-%dT%H:%M:%SZ", &tm) != nullptr)
        {
            time_t t = mktime(&tm);
            struct timeval tv = {0};
            tv.tv_sec = t;
            tv.tv_usec = 0;
            settimeofday(&tv, nullptr);
            HAL_LOG_INFO("HEATER", "Time synced: %s", timestamp);
        }
        else
        {
            HAL_LOG_WARN("HEATER", "Failed to parse timestamp: %s", timestamp);
        }
    }

    // =============================================================================
    // STATIC CALLBACKS
    // =============================================================================

    void HeaterDevice::onCloudStateChange(cloud::CloudState oldState,
                                          cloud::CloudState newState,
                                          void *ctx)
    {
        auto *self = static_cast<HeaterDevice *>(ctx);

        HAL_LOG_INFO("HEATER", "Cloud: %s -> %s",
                     cloud::cloudStateToString(oldState),
                     cloud::cloudStateToString(newState));

        if (newState == cloud::CloudState::Online)
        {
            self->publisher_.resetInfoPublished();
            self->infoPublished_ = false;
            self->integrations_.publishStatus();
        }
    }

    void HeaterDevice::onClaimPin(const char *pin, uint32_t expiresInSeconds, void *ctx)
    {
        auto *self = static_cast<HeaterDevice *>(ctx);

        HAL_LOG_INFO("HEATER", "Claim PIN: %s (expires in %ds)", pin, expiresInSeconds);

        if (self->userClaimPinCallback_)
        {
            self->userClaimPinCallback_(pin, expiresInSeconds);
        }
    }

    void HeaterDevice::onClaimComplete(const char *deviceId, void *ctx)
    {
        (void)ctx;
        HAL_LOG_INFO("HEATER", "Claim complete: deviceId=%s", deviceId);
    }

    void HeaterDevice::onUnclaimed(void *ctx)
    {
        (void)ctx;
        HAL_LOG_WARN("HEATER", "Device not claimed");
    }

} // namespace iheaterlink

#endif // ESP32 || ESP_PLATFORM
