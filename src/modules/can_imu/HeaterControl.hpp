#pragma once

#include <cmath>
#include <cstdint>

namespace canimu {

// One controller per physical heater. Feedback timestamps identify driver reads.
class HeaterControl {
public:
    enum Fault : uint8_t {
        None,
        InvalidTemperature,
        StaleTemperature,
        Overtemperature,
        WarmupTimeout
    };

    void start(uint64_t now)
    {
        *this = HeaterControl{};
        _started = now;
    }

    void update(uint64_t now, float temperature, uint64_t measured_at, bool enabled,
                float target, float kp = 0.10f, float ki = 0.01f)
    {
        const bool fresh = measured_at && measured_at <= now && now - measured_at < TemperatureTimeoutUs;
        const bool finite = std::isfinite(temperature);
        const bool interrupted = _updated && (now < _updated || now - _updated >= TemperatureTimeoutUs);
        const float dt = _updated && now >= _updated ? fminf(float(now - _updated) * 1e-6f, 0.05f) : 0.0f;
        _updated = now;
        _duty = 0;
        _on = false;

        if (enabled && now >= _started && now - _started >= StartupGraceUs && _fault == None) {
            if (!finite) {
                _fault = InvalidTemperature;
            } else if (!fresh) {
                _fault = StaleTemperature;
            } else if (temperature >= MaximumTemperature) {
                _fault = Overtemperature;
            } else if (now - _started >= WarmupTimeoutUs && temperature < target - 5) {
                _fault = WarmupTimeout;
            }

            if (_fault == None) {
                const float error = target - temperature;
                const float proposed = _integral + ki * error * dt;
                const float raw = kp * error + proposed;

                // Integrate within the duty limits or while moving out of saturation.
                if ((raw >= 0 && raw <= MaximumDuty) || (raw > MaximumDuty && error < 0)
                    || (raw < 0 && error > 0)) {
                    _integral = fminf(MaximumDuty, fmaxf(0.0f, proposed));
                }

                _duty = fminf(MaximumDuty, fmaxf(0.0f, kp * error + _integral));
                _on = (now % PwmPeriodUs) < uint64_t(_duty * PwmPeriodUs);
            }
        }

        if (!enabled || _fault != None) {
            _integral = 0;
        }

        // A gap in observation breaks the continuous stability interval.
        if (interrupted) {
            _warm_since = 0;
        }

        if (finite && fresh && _fault == None && fabsf(temperature - target) <= 1.0f) {
            if (!_warm_since) {
                _warm_since = now;
            }
        } else {
            _warm_since = 0;
        }
    }

    bool on() const { return _on; }
    float duty() const { return _duty; }
    Fault fault() const { return _fault; }

    bool warm(uint64_t sample_end) const
    {
        return _warm_since && sample_end >= _warm_since && sample_end - _warm_since >= StabilityDurationUs;
    }

private:
    static constexpr uint64_t TemperatureTimeoutUs = 200000;
    static constexpr uint64_t StartupGraceUs = 1000000;
    static constexpr uint64_t WarmupTimeoutUs = 300000000;
    static constexpr uint64_t StabilityDurationUs = 10000000;
    static constexpr uint64_t PwmPeriodUs = 50000;
    static constexpr float MaximumTemperature = 60.0f;
    static constexpr float MaximumDuty = 0.6f;

    uint64_t _started = 0;
    uint64_t _updated = 0;
    uint64_t _warm_since = 0;
    float _integral = 0;
    float _duty = 0;
    Fault _fault = None;
    bool _on = false;
};

} // namespace canimu
