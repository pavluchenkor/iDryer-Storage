# Как добавить sensor

**Sensor** — продуктовый источник данных: датчик температуры, влажности, веса, RFID, тока, давления и т. п. Sensor живёт в product code (`src/`), не в библиотеке. Библиотека предоставляет транспорт (`MqttClient`, `DevicePublisher`), но не знает, какой именно датчик у вас стоит.

Эталонный пример — `Sht31ClimateSensor` в Storage Link (`src/storage/sensors/`).

## Шаблон sensor-класса

Минимальный контракт, которого придерживается Storage Link:

```cpp
// src/myproduct/sensors/IMySensor.h
#pragma once
#include <stdint.h>

struct MySensorReading {
    float    value = 0.0f;
    uint32_t ts_ms = 0;
    bool     ok    = false;
    int      err   = 0;
};

class IMySensor {
public:
    virtual ~IMySensor() = default;
    virtual bool             begin() = 0;
    virtual void             tick(uint32_t nowMs) = 0;
    virtual MySensorReading  get() const = 0;
};
```

Реализация:

```cpp
// src/myproduct/sensors/MySensor.h
class MySensor : public IMySensor {
public:
    bool begin() override;
    void tick(uint32_t nowMs) override;
    MySensorReading get() const override { return last_; }
private:
    MySensorReading last_;
};
```

Принципы:

- **Не блокируйте `loop()`**. Если железо асинхронное (как SHT31 с `requestData`/`readData`) — используйте `tick(now)` как стейт-машину.
- **`begin()` возвращает `bool`**. `false` — датчик не найден. Продукт должен корректно работать без датчика (как Storage Link при отсутствии SHT31).
- **`get()` возвращает последний снимок**. Нет блокировки, нет ожидания.

## Шаблон публикации показаний

Sensor сам ничего не публикует. Публикацию делает отдельный класс — *publisher* — который читает sensor и отправляет в transport.

Эталон — `StorageTelemetryPublisher` (`src/storage/telemetry/`):

```cpp
// src/myproduct/telemetry/MyTelemetryPublisher.h
#include "../sensors/IMySensor.h"
#include <local_access/device_publisher.h>

class MyTelemetryPublisher {
public:
    MyTelemetryPublisher(IMySensor* sensor,
                         idryer::DevicePublisher* pub,
                         uint32_t intervalMs = 10000)
        : sensor_(sensor), pub_(pub), intervalMs_(intervalMs) {}

    void loop(uint32_t nowMs) {
        if (nowMs - lastPublishMs_ < intervalMs_) return;
        lastPublishMs_ = nowMs;

        MySensorReading r = sensor_->get();
        if (!r.ok) return;

        StaticJsonDocument<128> doc;
        JsonArray units = doc.createNestedArray("units");
        JsonObject u    = units.createNestedObject();
        u["unitId"]      = "U1";
        u["temperature"] = r.value;
        pub_->publishTelemetry(doc);
    }
private:
    IMySensor*               sensor_;
    idryer::DevicePublisher* pub_;
    uint32_t                 intervalMs_;
    uint32_t                 lastPublishMs_ = 0;
};
```

Принципы:

- Publisher принимает указатель на sensor и `DevicePublisher` через конструктор.
- Если LocalAccess не используется — принимайте `MqttClient*` напрямую.
- `loop(now)` неблокирующая, дросселирована интервалом.
- Schema публикуемого JSON — продуктовое решение (см. бэкенд-контракт).

## Включение в composition root

```cpp
// в main.cpp продукта

#include "myproduct/sensors/MySensor.h"
#include "myproduct/telemetry/MyTelemetryPublisher.h"

static MySensor              s_sensor;
static MyTelemetryPublisher  s_telemetry(&s_sensor, &s_pub);   // s_pub = DevicePublisher
static bool                  s_sensorOk = false;

void setup() {
    // ... обычная инициализация ...
    Wire.begin(SDA_PIN, SCL_PIN);
    s_sensorOk = s_sensor.begin();   // false — устройство работает без датчика
    // ...
}

void loop() {
    s_runtime.loop();
    s_local.loop();
    if (s_sensorOk) {
        s_sensor.tick(millis());
        s_telemetry.loop(millis());
    }
}
```

## Sensor через `commands/invoke`

Если показания нужны *по запросу*, а не периодически, добавьте action в `handleCommand`:

```cpp
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    if (strcmp(action, "sensor.read") == 0) {
        MySensorReading r = s_sensor.get();
        StaticJsonDocument<128> doc;
        doc["value"] = r.value;
        doc["ok"]    = r.ok;
        s_pub.publishTelemetry(doc);
        return true;
    }
    return false;
}
```

`s_dispatcher.setInvokeHandler(onInvoke, nullptr)` — и бэкенд может вызвать `{"action":"sensor.read"}` вместо ожидания периодической публикации.

## Чего не делаем

- Не подключаем sensor напрямую к `MqttClient` через прокидывание указателя в конструктор `MqttClient`. У `MqttClient` нет такого API — и не должно быть. Sensor → publisher → transport остаётся раздельной цепочкой.
- Не складываем sensor внутрь `IProfile`. `IProfile` отвечает за config/info/lifecycle, не за сенсорные данные. Это разные оси ответственности.
- Не делаем sensor `static` на уровне библиотеки. Все продуктовые объекты — в product code.

## Связанные документы

- [02-architecture/03-data-flow.md](../02-architecture/03-data-flow.md) — общая схема потоков.
- [12-patterns/04-data-flow.md](04-data-flow.md) — рецепты передачи данных между частями.
- [03-mqtt/02-topics-and-messages.md](../03-mqtt/02-topics-and-messages.md) — формат telemetry payload.
