#include "Sht31ClimateSensor.h"

static constexpr int kErrOk         = 0;
static constexpr int kErrNotFound   = 1;
static constexpr int kErrNoData     = 2;
static constexpr int kErrReadFail   = 3;
static constexpr int kErrOutOfRange = 4;

static constexpr uint8_t kCandidates[] = {0x44, 0x45};

Sht31ClimateSensor::Sht31ClimateSensor(TwoWire* bus, uint32_t pollMs)
    : bus_(bus), dev_(0x44, bus), pollMs_(pollMs) {}

bool Sht31ClimateSensor::begin() {
    if (!bus_) { setError(0, kErrNotFound); return false; }

    // Probe 0x44 then 0x45 — use the first that ACKs on the I2C bus
    uint8_t found = 0;
    for (uint8_t addr : kCandidates) {
        bus_->beginTransmission(addr);
        if (bus_->endTransmission() == 0) { found = addr; break; }
    }
    if (!found) { setError(0, kErrNotFound); return false; }

    dev_ = SHT31(found, bus_);
    if (!dev_.begin()) { setError(0, kErrNotFound); return false; }

    foundAddr_ = found;
    last_      = {};
    dev_.requestData();
    return true;
}

void Sht31ClimateSensor::tick(uint32_t nowMs) {
    if ((nowMs - prevMs_) < pollMs_) return;
    prevMs_ = nowMs;

    if (!dev_.dataReady()) { setError(nowMs, kErrNoData);   return; }
    if (!dev_.readData())  { setError(nowMs, kErrReadFail); dev_.requestData(); return; }

    float t = dev_.getTemperature();
    float h = dev_.getHumidity();

    if (isnan(t) || isnan(h) || t < -40.0f || t > 125.0f || h < 0.0f || h > 100.0f) {
        setError(nowMs, kErrOutOfRange);
        dev_.requestData();
        return;
    }

    last_.temperature = t;
    last_.humidity    = h;
    last_.pressure    = NAN;
    last_.ts_ms       = nowMs;
    last_.ok          = true;
    last_.err         = kErrOk;

    dev_.requestData();
}

void Sht31ClimateSensor::setError(uint32_t nowMs, int errCode) {
    last_.temperature = NAN;
    last_.humidity    = NAN;
    last_.pressure    = NAN;
    last_.ts_ms       = nowMs;
    last_.ok          = false;
    last_.err         = errCode;
}
