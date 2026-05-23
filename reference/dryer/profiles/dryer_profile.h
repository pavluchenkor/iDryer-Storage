#pragma once

#include "../IProfile.h"
#include "../../uart/uart_bridge.h"
#include "../../config/config_manager.h"
#include "../../mqtt/mqtt_client.h"
#include "../../cloud/cloud_state_machine.h"

namespace idryer {

// DryerProfile — IProfile for ESP32-C3 + RP2040 Dryer Link.
//
// Identity path (two-MCU bridge):
//   loop() sends periodic HelloRequest until Hello arrives from RP2040.
//   onHello() calls cloud_->setMcuSerial() so CloudStateMachine can proceed
//   from WaitingForMcuSerial → Provisioning.
//
// Data path:
//   Telemetry / Status / Weights → publishTelemetry / publishStatus via MqttClient.
//   ConfigPush chunks (MCU → ESP) → publishConfigRaw (raw JSON to config topic).
//   applyConfig(id, val) → ConfigSender → UartBridge::sendConfigPushChunk (config/delta).
class DryerProfile : public IProfile {
public:
    DryerProfile(UartBridge* uart, MqttClient* mqtt, cloud::CloudStateMachine* cloud);

    void onOnline() override;
    void loop() override;
    void buildInfoJson(char* buf, size_t len) const override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;

    // Wire UART handlers into bridge — call before runtime->begin()
    void wireHandlers();

private:
    void onHello(const UartHelloPayload& p, const UartFrameHeader& h);
    void onTelemetry(const UartTelemetryPayload& p, const UartFrameHeader& h);
    void onStatus(const UartStatusPayload& p, const UartFrameHeader& h);
    void onWeights(const UartWeightsPayload& p, const UartFrameHeader& h);
    void onConfigChunk(const UartConfigChunkPayload& p, uint8_t dataLen, const UartFrameHeader& h);
    void sendHeartbeat();
    void sendHelloRequest();

    UartBridge*                uart_;
    MqttClient*                mqtt_;
    cloud::CloudStateMachine*  cloud_;

    UartHelloPayload helloPayload_{};
    bool             helloReceived_      = false;
    bool             cloudOnline_        = false;
    uint32_t         lastHelloRequestMs_ = 0;
    uint32_t         lastHeartbeatMs_    = 0;

    ConfigReceiver configReceiver_;
    ConfigSender   configSender_;
};

} // namespace idryer
