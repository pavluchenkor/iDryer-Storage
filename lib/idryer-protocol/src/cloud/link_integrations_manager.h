/**
 * @file link_integrations_manager.h
 * @brief Оркестратор LINK-интеграций (HA / Bambu / Moonraker).
 *
 * Принимает разобранный payload `commands/link_integration` и
 * `commands/bambu_apply`, обновляет NVS через `LinkIntegrationsStore`,
 * публикует сводный снимок в `integrations/status`.
 *
 * Поведение клиентских подключений (реальный Bambu / Moonraker / HA) —
 * в отдельных классах, будет добавлено в последующих фазах. В Фазе 1
 * менеджер хранит и публикует статус, клиенты — заглушки.
 *
 * @see docs/ru/07-features/03-link-integrations-overview.md
 */

#pragma once

#include "link_integrations_types.h"
#include "link_integrations_store.h"
#include "bambu_client.h"
#include "moonraker_client.h"
#include "ha_integration_adapter.h"
#include "../uart/uart_protocol.h"  // для DryerUart::DeviceType

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "../mqtt/mqtt_client.h"  // MqttClient используется через указатель; полный тип
                                  // нужен потребителям заголовка (IdryerDevice и т.п.).
#include <ArduinoJson.h>

namespace idryer {
namespace cloud {

/// Оркестратор трёх LINK-интеграций.
class LinkIntegrationsManager
{
public:
    /// @param mqtt  MQTT-клиент устройства (публикация `integrations/status`).
    /// @param store NVS-хранилище (live-владение не передаётся).
    LinkIntegrationsManager(::MqttClient* mqtt, LinkIntegrationsStore* store);

    /// Загружает все секции из NVS. Публикует первый `integrations/status`
    /// (когда MQTT уже онлайн — вызов повторно безопасен).
    void begin();

    /// Обработчик `commands/link_integration`. Диспетчер по полю `type`:
    ///   - `"ha"`        → парсит и сохраняет секцию HA.
    ///   - `"bambu"`     → парсит и сохраняет секцию Bambu.
    ///   - `"moonraker"` → парсит и сохраняет секцию Moonraker.
    /// После сохранения публикует обновлённый `integrations/status`.
    void handleLinkIntegrationCommand(JsonObjectConst data);

    /// Обработчик `commands/bambu_apply`. В Фазе 1 — только логирование
    /// и обновление `integrations/status.bambu.lastApply` как `"not_implemented"`.
    /// Реальная отправка `ams_filament_setting` в принтер — Фаза 2
    /// (`BambuClient::applyFilament`).
    void handleBambuApplyCommand(JsonObjectConst data);

    /// Сменить активную интеграцию (вызывается из колбэка `ConfigPush`
    /// элемента меню `activeIntegration` либо локально). Сохраняет в NVS
    /// и публикует статус.
    void setActive(ActiveIntegration active);

    /// Текущее выбранное значение (из NVS).
    ActiveIntegration getActive() const { return common_.active; }

    /// Вызывается из главного цикла. В Фазе 1 ничего не делает —
    /// клиенты появятся в последующих фазах.
    void loop();

    /// Ручная пересборка и публикация `integrations/status` (например,
    /// после переподключения к MQTT-брокеру iDryer).
    void publishStatus();

    // ------------------------------------------------------------------------
    // Потребительские колбэки (прикладная прошивка LINK iHeater подписывается
    // сюда, чтобы пробросить данные в MCU через UART).
    // ------------------------------------------------------------------------

    /// Колбэк «целевая температура камеры изменилась».
    /// Вызывается при изменении `VIRTUAL_CHAMBER.target` на Klipper,
    /// независимо от того, кто изменил — слайсер через M141 или пользователь.
    ///
    /// @see docs/ru/07-features/06-moonraker-printer.md
    void setChamberTargetCallback(MoonrakerClient::ChamberTargetCallback cb);

    /// Колбэк «общий статус принтера изменился» (для отображения прогресса,
    /// температур, filename на экране устройства).
    void setMoonrakerStatusCallback(MoonrakerClient::StatusChangeCallback cb);

    /// Предпочтительный колбэк для iHeater: VIRTUAL_CHAMBER данные
    /// (target + temperature + hasSensor + available) при каждом
    /// изменении. MCU iHeater использует их для PID (target = setpoint,
    /// temperature = feedback, если hasSensor).
    void setVirtualChamberCallback(MoonrakerClient::VirtualChamberCallback cb);

    /// Доступ к последнему снимку статуса Moonraker.
    const MoonrakerStatus& moonrakerStatus() const { return moonrakerClient_.status(); }

    /// Колбэк «статус принтера Bambu изменился» (Reader-режим, iHeater).
    /// Прошивка iHeater использует его для передачи данных в MCU через UART.
    void setBambuPrinterStatusCallback(BambuClient::PrinterStatusCallback cb);

    /// Последний снимок статуса принтера Bambu (Reader-режим).
    const BambuPrinterStatus& bambuPrinterStatus() const { return bambuClient_.printerStatus(); }

    /// Передаёт LINK-serial как Client ID для подключения к HA-брокеру.
    /// Вызвать **до** `begin()` (до первой попытки поднять HA-клиент).
    void setHaClientId(const char* clientId) { haClient_.setClientId(clientId); }

    /// Доступ к внутреннему `HaMqttClient` — прикладной код поверх него
    /// создаёт `HaPublisher` для публикации сенсоров устройства.
    ha::HaMqttClient* haMqttClient() { return haClient_.mqttClient(); }

    // ------------------------------------------------------------------------
    // deviceType-aware поведение (для iHeater vs iDryer)
    // ------------------------------------------------------------------------

    /// Сообщает менеджеру тип устройства (из `HelloPayload.deviceType`).
    /// Влияет на режимы клиентов: например, Bambu для `Heater`/`IHeaterLink`
    /// работает в Reader-режиме (подписка на report) без Writer-пути.
    /// Если не вызывать — считается `Dryer` (Writer-режим Bambu включён).
    ///
    /// @see docs/ru/07-features/05-bambu-integration.md
    void setDeviceType(DryerUart::DeviceType deviceType);
    DryerUart::DeviceType deviceType() const { return deviceType_; }

private:
    // Сериализация одной секции в JSON.
    void serializeHaSection(JsonObject section) const;
    void serializeBambuSection(JsonObject section) const;
    void serializeMoonrakerSection(JsonObject section) const;

    // Текущее состояние секции для поля `state`.
    // В Фазе 1 оно вычисляется только из `active` + `configured()`:
    //   - active != эта секция               → Disabled
    //   - active == эта секция + !configured → ConfigMissing
    //   - active == эта секция + configured  → Idle (клиент ещё не реализован)
    // В следующих фазах клиент сможет сам выставлять Connecting/Online/Error.
    IntegrationState computeHaState() const;
    IntegrationState computeBambuState() const;
    IntegrationState computeMoonrakerState() const;

    // Парсинг payload в структуру. Возвращает `false`, если тело невалидно.
    bool parseHa(JsonObjectConst data, HaConfig& out) const;
    bool parseBambu(JsonObjectConst data, BambuConfig& out) const;
    bool parseMoonraker(JsonObjectConst data, MoonrakerConfig& out) const;
    bool parseBambuApply(JsonObjectConst data, BambuApplyPayload& out) const;

    // Хелпер: копирует строковое поле JSON в C-буфер с обрезанием.
    static void copyField(JsonObjectConst data, const char* key, char* buf, size_t bufSize);

    ::MqttClient*           mqtt_;
    LinkIntegrationsStore*  store_;

    // Встроенный клиент Bambu (Фаза 2 — Writer-режим).
    // Живёт в поле, поднимается когда active == Bambu + configured.
    BambuClient             bambuClient_;

    // Встроенный клиент Moonraker (Фаза 3 — WebSocket JSON-RPC).
    // Живёт в поле, поднимается когда active == Moonraker + configured.
    MoonrakerClient         moonrakerClient_;

    // HA-адаптер (Фаза 4). Поднимается когда active == Ha + configured.
    HaIntegrationAdapter    haClient_;

    // Тип устройства (из HelloPayload.deviceType). Влияет на режим Bambu.
    // По умолчанию Dryer — Writer-режим Bambu включён.
    DryerUart::DeviceType   deviceType_ = DryerUart::DeviceType::Dryer;

    // Кэш последних загруженных конфигов — чтобы не ходить в NVS
    // при каждой публикации status.
    HaConfig         ha_;
    BambuConfig      bambu_;
    MoonrakerConfig  moonraker_;
    CommonConfig     common_;

    // Последнее известное сообщение об ошибке в каждой секции
    // (для `lastError` в status). В Фазе 1 обычно пусто.
    char haLastError_[96]        = {0};
    char bambuLastError_[96]     = {0};
    char moonrakerLastError_[96] = {0};

    // Периодическая публикация integrations/status (raz v 30s) чтобы
    // свежие значения temperature/progress и т.п. попадали в retained
    // snapshot для UI — без спама на каждое изменение.
    static constexpr uint32_t kPeriodicStatusIntervalMs = 30000;
    uint32_t lastStatusPublishMs_ = 0;

    // Последний результат Bambu apply (для `bambu.lastApply` в status).
    bool    bambuHasLastApply_   = false;
    char    bambuLastApplyAt_[24]       = {0};  // ISO 8601
    char    bambuLastApplyResult_[16]   = {0};  // "ok" / "failed"
    char    bambuLastApplySpoolId_[40]  = {0};
    uint8_t bambuLastApplyAmsId_        = 0;
    uint8_t bambuLastApplyTrayId_       = 0;

    // Применяет текущий `common_.active` к клиентам:
    // активная интеграция поднимается (configure), неактивные опускаются.
    void applyActiveIntegration();
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
