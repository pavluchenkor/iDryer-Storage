#if defined(ESP32) || defined(ESP_PLATFORM)

#include "dryer_profile.h"
#include "../../hal/hal_types.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

namespace idryer {

DryerProfile::DryerProfile(UartBridge* uart, MqttClient* mqtt,
                            cloud::CloudStateMachine* cloud)
    : uart_(uart), mqtt_(mqtt), cloud_(cloud)
{}

void DryerProfile::wireHandlers() {
    uart_->setHelloHandler([this](const UartHelloPayload& p, const UartFrameHeader& h) {
        onHello(p, h);
    });
    uart_->setTelemetryHandler([this](const UartTelemetryPayload& p, const UartFrameHeader& h) {
        onTelemetry(p, h);
    });
    uart_->setStatusHandler([this](const UartStatusPayload& p, const UartFrameHeader& h) {
        onStatus(p, h);
    });
    uart_->setWeightsHandler([this](const UartWeightsPayload& p, const UartFrameHeader& h) {
        onWeights(p, h);
    });
    uart_->setConfigChunkHandler([this](const UartConfigChunkPayload& p, uint8_t dataLen,
                                        const UartFrameHeader& h) {
        onConfigChunk(p, dataLen, h);
    });
}

// ============================================================================
// IProfile interface
// ============================================================================

void DryerProfile::onOnline() {
    cloudOnline_ = true;
    HAL_LOG_INFO("DRYER", "Cloud online. helloReceived=%d", helloReceived_);
}

void DryerProfile::loop() {
    uart_->loop();

    // Send periodic HelloRequest until RP2040 responds with Hello.
    // Once Hello is received and mcuSerial is set, CloudStateMachine
    // exits WaitingForMcuSerial and proceeds to Provisioning.
    if (!helloReceived_) {
        uint32_t now = HAL_MILLIS();
        if (now - lastHelloRequestMs_ >= UART_HELLO_INTERVAL_MS) {
            sendHelloRequest();
            lastHelloRequestMs_ = now;
        }
    }

    // Periodic heartbeat to MCU once cloud is online
    if (cloudOnline_ && helloReceived_) {
        uint32_t now = HAL_MILLIS();
        if (now - lastHeartbeatMs_ >= UART_HEARTBEAT_MS) {
            sendHeartbeat();
            lastHeartbeatMs_ = now;
        }
    }
}

void DryerProfile::buildInfoJson(char* buf, size_t len) const {
    DynamicJsonDocument doc(512);

    if (helloReceived_) {
        uint32_t fw = helloPayload_.firmwareVersion;
        char fwStr[16];
        snprintf(fwStr, sizeof(fwStr), "%u.%u.%u",
                 (fw >> 16) & 0xFF, (fw >> 8) & 0xFF, fw & 0xFF);
        doc["firmwareVersion"] = fwStr;
        doc["hardwareVersion"] = helloPayload_.hardwareVersion;
        doc["mcuSerial"]       = helloPayload_.mcuSerial;
        doc["unitsCount"]      = helloPayload_.unitsCount;

        switch (static_cast<UartDeviceType>(helloPayload_.deviceType)) {
            case UartDeviceType::Dryer:  doc["deviceType"] = "dryer";  break;
            case UartDeviceType::Heater: doc["deviceType"] = "heater"; break;
            default:                     doc["deviceType"] = "dryer";  break;
        }
    } else {
        doc["firmwareVersion"] = "0.0.0";
        doc["hardwareVersion"] = "unknown";
        doc["deviceType"]      = "dryer";
        doc["mcuSerial"]       = "";
    }

    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;

    serializeJson(doc, buf, len);
}

void DryerProfile::getConfig(JsonDocument& out) {
    // Returns minimal placeholder; full config comes from MCU via ConfigPush.
    out["deviceType"] = "dryer";
    char ts[32]; MqttClient::getIsoTimestamp(ts);
    out["timestamp"]  = ts;
}

bool DryerProfile::applyConfig(int id, int val) {
    // Build minimal JSON delta and send to MCU via ConfigPush
    char json[64];
    int  len = snprintf(json, sizeof(json), "{\"id\":%d,\"val\":%d}", id, val);
    if (len <= 0 || len >= (int)sizeof(json)) return false;

    uint16_t tid     = ConfigSender::generateTransferId();
    uint16_t chunks  = configSender_.send(json, static_cast<uint16_t>(len), tid,
        [this](const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
            return uart_->sendConfigPushChunk(p, payloadLen, flags);
        });

    if (chunks > 0 && mqtt_) {
        // Publish delta so portal sees the accepted value
        mqtt_->publishConfigDelta(json, static_cast<size_t>(len));
    }
    return chunks > 0;
}

// ============================================================================
// UART handlers
// ============================================================================

void DryerProfile::onHello(const UartHelloPayload& p, const UartFrameHeader& /*h*/) {
    helloPayload_  = p;
    helloReceived_ = true;

    HAL_LOG_INFO("DRYER", "Hello from MCU: type=%u fw=0x%08X units=%u serial=%s",
                 p.deviceType, p.firmwareVersion, p.unitsCount, p.mcuSerial);

    // Two-MCU identity path: pass MCU serial to CloudStateMachine.
    // CSM exits WaitingForMcuSerial and proceeds to Provisioning.
    cloud_->setMcuSerial(p.mcuSerial);

    // Acknowledge Hello
    UartHelloAckPayload ack{};
    ack.ipAddress = 0;
    ack.ssid[0]   = '\0';
    uart_->sendHelloAck(ack);
}

void DryerProfile::onTelemetry(const UartTelemetryPayload& p, const UartFrameHeader& /*h*/) {
    if (!mqtt_) return;
    DynamicJsonDocument doc(512);
    JsonArray units = doc.createNestedArray("units");
    for (uint8_t i = 0; i < p.count && i < 4; ++i) {
        const auto& u = p.units[i];
        JsonObject obj = units.createNestedObject();
        obj["id"]          = u.unitId;
        obj["tempC"]       = u.temperatureC10 / 10.0f;
        obj["humidityPct"] = u.humidityPct10  / 10.0f;
        obj["heaterPct"]   = u.heaterPowerPct;
        obj["fanOn"]       = (bool)u.fanOn;
    }
    mqtt_->publishTelemetry(doc);
}

void DryerProfile::onStatus(const UartStatusPayload& p, const UartFrameHeader& /*h*/) {
    if (!mqtt_) return;
    DynamicJsonDocument doc(768);
    doc["uptime"] = p.uptime;
    JsonArray units = doc.createNestedArray("units");
    for (uint8_t i = 0; i < p.count && i < 4; ++i) {
        const auto& u = p.units[i];
        JsonObject obj = units.createNestedObject();
        obj["id"]             = u.unitId;
        obj["mode"]           = static_cast<uint8_t>(u.mode);
        obj["session"]        = u.sessionNum;
        obj["elapsed"]        = u.elapsedSeconds;
        obj["totalRemaining"] = u.totalRemainingSeconds;
        if (u.mode == UartDryerMode::Profile) {
            obj["stage"]          = u.currentStage;
            obj["totalStages"]    = u.totalStages;
            obj["stageElapsed"]   = u.stageElapsedSeconds;
            obj["stageRemaining"] = u.stageRemainingSeconds;
        }
    }
    mqtt_->publishStatus(doc);
}

void DryerProfile::onWeights(const UartWeightsPayload& p, const UartFrameHeader& /*h*/) {
    if (!mqtt_) return;
    DynamicJsonDocument doc(256);
    JsonArray arr = doc.createNestedArray("weights");
    for (uint8_t i = 0; i < p.count && i < 4; ++i) {
        JsonObject obj = arr.createNestedObject();
        obj["sensorId"]    = p.weights[i].sensorId;
        obj["unitId"]      = p.weights[i].unitId;
        obj["weightGrams"] = p.weights[i].weightGramsC10 / 10.0f;
    }
    mqtt_->publishTelemetry(doc);
}

void DryerProfile::onConfigChunk(const UartConfigChunkPayload& p, uint8_t dataLen,
                                  const UartFrameHeader& h) {
    ConfigFragResult result = configReceiver_.processFragment(p, dataLen, h.flags);

    if (result == ConfigFragResult::Complete) {
        HAL_LOG_INFO("DRYER", "Config from MCU: %u bytes → config topic",
                     configReceiver_.getLength());
        if (mqtt_) {
            // Publish raw JSON directly to config topic — no wrapping, matches portal contract
            mqtt_->publishConfigRaw(configReceiver_.getJson(), configReceiver_.getLength());
        }
        configReceiver_.reset();
    } else if (result != ConfigFragResult::Ok) {
        HAL_LOG_WARN("DRYER", "Config fragment error: %u (tid=%u)",
                     static_cast<uint8_t>(result), configReceiver_.transferId());
        configReceiver_.reset();
    }
}

void DryerProfile::sendHeartbeat() {
    UartHeartbeatPayload hb{};
    hb.uptimeSeconds = HAL_MILLIS() / 1000;
    hb.cloudState    = static_cast<uint8_t>(cloudOnline_
                       ? UartLinkCloudState::Online
                       : UartLinkCloudState::MqttConnecting);
    uart_->sendHeartbeat(hb);
}

void DryerProfile::sendHelloRequest() {
    UartHelloPayload req{};
    req.role = UartRole::HelloRequest;
    uart_->sendHello(req, false);
    HAL_LOG_DEBUG("DRYER", "HelloRequest sent");
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
