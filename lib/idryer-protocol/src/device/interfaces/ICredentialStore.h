/**
 * @file ICredentialStore.h
 * @brief Интерфейс постоянного хранения `DeviceIdentity`.
 */

#pragma once

#include "../../core/types.h"

namespace idryer {

class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;

    /// Подготавливает backend хранилища.
    virtual bool begin() = 0;

    /// Возвращает `true`, если загружены валидные credentials.
    virtual bool load(DeviceIdentity& identity) = 0;

    virtual bool save(const DeviceIdentity& identity) = 0;

    /// Полный сброс сохранённых credentials.
    virtual void clear() = 0;
};

} // namespace idryer
