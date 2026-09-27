#pragma once

#include <cmath>
#include <cstdint>

namespace canimu {

// One controller per physical heater. Temperature timestamps identify actual
// driver reads, not publications that may repeat a cached temperature.
class HeaterControl {
public:
 enum Fault : uint8_t { None, InvalidTemperature, StaleTemperature, Overtemperature, WarmupTimeout };

 void start(uint64_t now) { *this = HeaterControl{}; _started = now; }

 void update(uint64_t now, float temperature, uint64_t measured_at, bool enabled, float target, float kp = 0.10f, float ki = 0.01f) {
  const bool fresh = measured_at && measured_at <= now && now - measured_at < 200000;
  const bool finite = std::isfinite(temperature);
  const bool interrupted = _updated && (now < _updated || now - _updated >= 200000);
  const float dt = _updated && now >= _updated ? fminf(float(now - _updated) * 1e-6f, 0.05f) : 0.0f;
  _updated = now;
  _duty = 0;
  _on = false;

  if (enabled && now >= _started && now - _started >= 1000000 && _fault == None) {
   if (!finite) { _fault = InvalidTemperature; }
   else if (!fresh) { _fault = StaleTemperature; }
   else if (temperature >= 60) { _fault = Overtemperature; }
   else if (now - _started >= 300000000 && temperature < target - 5) { _fault = WarmupTimeout; }
   if (_fault == None) {
    const float error = target - temperature;
    const float proposed = _integral + ki * error * dt;
    const float raw = kp * error + proposed;
    if ((raw >= 0 && raw <= 0.6f) || (raw > 0.6f && error < 0) || (raw < 0 && error > 0)) {
     _integral = fminf(0.6f, fmaxf(0.0f, proposed));
    }
    _duty = fminf(0.6f, fmaxf(0.0f, kp * error + _integral));
    _on = (now % 50000) < uint64_t(_duty * 50000);
   }
  }

  if (!enabled || _fault != None) { _integral = 0; }
  // A gap in observation breaks the continuous ten-second stability interval.
  if (interrupted) { _warm_since = 0; }
  if (finite && fresh && _fault == None && fabsf(temperature - target) <= 1.0f) {
   if (!_warm_since) { _warm_since = now; }
  } else { _warm_since = 0; }
 }

 bool on() const { return _on; }
 float duty() const { return _duty; }
 Fault fault() const { return _fault; }
 bool warm(uint64_t sample_end) const {
  return _warm_since && sample_end >= _warm_since && sample_end - _warm_since >= 10000000;
 }

private:
 uint64_t _started = 0, _updated = 0, _warm_since = 0;
 float _integral = 0, _duty = 0;
 Fault _fault = None;
 bool _on = false;
};

} // namespace canimu
