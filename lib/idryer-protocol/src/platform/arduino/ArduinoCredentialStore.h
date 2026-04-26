/**
 * @file ArduinoCredentialStore.h
 * @brief Arduino/ESP32 реализация `ICredentialStore` через NVS.
 */

#pragma once

#include "../../device/interfaces/ICredentialStore.h"
#include <Preferences.h>

namespace idryer {

class ArduinoCredentialStore : public ICredentialStore {
public:
    bool begin() override;
    bool load(DeviceIdentity& identity) override;
    bool save(const DeviceIdentity& identity) override;
    void clear() override;

private:
    static constexpr const char* kNamespace = "idryer";
    Preferences prefs_;
};

} // namespace idryer
