# `ICommandSink` - an extension point for applying commands

`CommandHandler` parses incoming MQTT commands and turns them into **UART payload structures** (`CommandPayload`, `ProfilePayload`, `ConfigChunkPayload`). What happens to those structures next is determined by the implementation of the **`ICommandSink`** interface.

This allows the same command parsing logic to work in two scenarios:

- **UART bridge (iDryer Link):** the sink sends payloads over UART to the MCU.
- **Standalone (iHeater Link II):** the sink applies the command locally and controls the heater/sensors directly on the ESP32.

!!! note
    Interface: `src/cloud/command_sink.h`. UART implementation: `src/cloud/uart_command_sink.h`. Consumer: `src/cloud/command_handler.cpp`.

---

## Interface

```cpp
namespace idryer::cloud {

class ICommandSink {
public:
    virtual ~ICommandSink() = default;

    virtual void sendCommand(const DryerUart::CommandPayload& payload,
                             bool ackRequired) = 0;

    virtual void sendProfileCommand(const DryerUart::ProfilePayload& payload,
                                    bool ackRequired) = 0;

    virtual void sendConfigPushChunk(const DryerUart::ConfigChunkPayload& payload,
                                     uint8_t dataLen, uint8_t flags) = 0;
};

}  // namespace idryer::cloud
```

The signatures match the corresponding `DryerUart::UartBridge` methods 1:1. Migrating existing consumers is purely mechanical.

---

## UART device implementation: `UartCommandSink`

It ships with the library. Used in configurations with a separate MCU.

File: `src/cloud/uart_command_sink.h`.

```cpp
#include <idryer_protocol.h>

DryerUart::UartBridge           uart(...);
idryer::cloud::UartCommandSink  sink(&uart);
idryer::cloud::CommandHandler   cmdHandler(&sink);

// On an incoming MQTT command:
cmdHandler.handleMqttCommand("drying", jsonData);
// → sink.sendCommand(...) → uart.sendCommand(...) → MCU
```

Null guard: if `uart == nullptr`, calls to `sendCommand`/`sendProfileCommand`/`sendConfigPushChunk` are silently ignored. This simplifies testing and dev configurations.

---

## Standalone implementation: your own sink

Application code writes its own sink that applies commands **locally** without going through UART. Example for iHeater Link II (ESP32 does everything):

```cpp
#include <idryer_protocol.h>

class LocalHeaterSink : public idryer::cloud::ICommandSink {
public:
    LocalHeaterSink(HeaterController& heater) : heater_(heater) {}

    void sendCommand(const DryerUart::CommandPayload& p, bool /*ack*/) override {
        using DryerUart::CommandCode;

        switch (p.command) {
        case CommandCode::Start:
            // p.arg0 = target temperature × 10
            heater_.setTargetTemperature(p.arg0 / 10.0f);
            heater_.start(p.arg1 /* duration minutes */);
            break;

        case CommandCode::Stop:
            heater_.stop();
            break;

        case CommandCode::ClearErrors:
            heater_.clearFaultLog();
            break;

        default:
            // Other commands are not needed in this product
            break;
        }
    }

    void sendProfileCommand(const DryerUart::ProfilePayload& p, bool) override {
        heater_.applyProfile(p);   // custom profile implementation
    }

    void sendConfigPushChunk(const DryerUart::ConfigChunkPayload& p,
                             uint8_t dataLen, uint8_t flags) override {
        // If the user changes a setting from the menu, apply it to the local config
        config_.applyChunk(p, dataLen, flags);
    }

private:
    HeaterController& heater_;
    LocalConfig       config_;
};

// Usage:
LocalHeaterSink               sink(heater);
idryer::cloud::CommandHandler cmdHandler(&sink);

mqtt.setCommandCallback([&](const char* cmd, JsonObjectConst data) {
    cmdHandler.handleMqttCommand(cmd, data);
});
```

The library knows nothing about `LocalHeaterSink` - it is application code.

---

## What `ICommandSink` does **not** do

- **It does not replace the UART protocol.** It forwards the same payload structures that `UartBridge` writes to the wire. In standalone configurations, those structures still remain the "command format" even if they do not travel over UART - this lets you reuse `CommandHandler` without changes.
- **It does not add a high-level API.** `ICommandSink::sendCommand(CommandPayload, ack)` is still low level. If you need an API like `startDrying(unitId, tempC, durationMin)`, build it on top of the sink or on top of the application code; that is a separate task.
- **It does not hide `UartBridge` from the application.** On devices with an MCU, the application still keeps `UartBridge` for telemetry reception; `UartCommandSink` only wraps the **outgoing** command channel.

---

## Lifecycle

The consumer (application) creates the sink and passes a pointer to the `CommandHandler` constructor. **The sink must live at least as long as `CommandHandler`.**

In the reference iDryer Link firmware, both are members of the `IdryerDevice` class:

```cpp
class IdryerDevice {
    DryerUart::UartBridge           uart_;
    idryer::cloud::UartCommandSink  sink_;       // declared before cmdHandler_
    idryer::cloud::CommandHandler   cmdHandler_; // sink_ passed to the constructor
    // ...
};
```

The sink is declared **before** `cmdHandler_` in the class body, so it is initialized first and destroyed last. Keep that rule.

---

## Extensions

The interface is minimal and can be extended if needed. If you need a separate branch of "on command arrival, do X + send to MCU", the best path is:

1. Keep `UartCommandSink` as the "transport".
2. Attach your own layer in front of it - for example, log all commands to NVS, collect metrics, or analyze parameters.

Example decorator:

```cpp
class LoggingSinkDecorator : public idryer::cloud::ICommandSink {
public:
    LoggingSinkDecorator(idryer::cloud::ICommandSink* inner) : inner_(inner) {}

    void sendCommand(const DryerUart::CommandPayload& p, bool ack) override {
        logCmd(p);
        inner_->sendCommand(p, ack);
    }
    // ...

private:
    idryer::cloud::ICommandSink* inner_;
};
```

---

## What's next

- [01-claiming-overview.md](01-claiming-overview.md) - the claiming process.
- [../04-mqtt/04-backend-to-device.md](../04-mqtt/04-backend-to-device.md) - which JSON commands become which payload structures.
- [../06-flows/](../06-flows/) - the full scenarios involving `CommandHandler` and the sink.
