#include "HeaterControl.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using canimu::HeaterControl;

int main()
{
 HeaterControl h;
 h.start(1);
 h.update(500000, 20, 500000, true, 48);
 assert(!h.on() && h.duty() == 0); // startup grace
 h.update(1000001, 20, 1000001, true, 48);
 assert(h.fault() == HeaterControl::None && h.on() && h.duty() <= 0.6f);
 h.update(1035000, 20, 1000001, true, 48);
 assert(!h.on()); // off portion of the 50 ms PWM period
 h.update(1200001, 20, 1000001, true, 48);
 assert(h.fault() == HeaterControl::StaleTemperature && !h.on() && h.duty() == 0);
 h.update(1250000, 20, 1250000, true, 48);
 assert(h.fault() == HeaterControl::StaleTemperature && !h.on()); // latched

 for (float temperature : {NAN, INFINITY, 60.0f, 70.0f}) {
  h.start(1);
  h.update(1000001, temperature, 1000001, true, 48);
  assert(h.fault() != HeaterControl::None && !h.on());
 }
 for (uint64_t measured_at : {uint64_t(0), uint64_t(1000002)}) {
  h.start(1);
  h.update(1000001, 20, measured_at, true, 48);
  assert(h.fault() == HeaterControl::StaleTemperature && !h.on());
 }
 h.start(1);
 h.update(300000001, 40, 300000001, true, 48);
 assert(h.fault() == HeaterControl::WarmupTimeout && !h.on());

 h.start(1);
 for (uint64_t t = 1000001; t <= 11000001; t += 50000) {
  h.update(t, 48, t, true, 48);
  if (t < 11000001) { assert(!h.warm(t)); }
 }
 assert(h.warm(11000001));
 h.update(11050001, 50, 11050001, true, 48);
 assert(!h.warm(11050001));
 for (uint64_t t = 11100001; t <= 21100001; t += 50000) { h.update(t, 48, t, true, 48); }
 assert(h.warm(21100001));
 h.update(21400001, 48, 21400001, true, 48);
 assert(!h.warm(21400001)); // fresh data after an observation gap cannot prove stability
 h.update(21450001, 60, 21450001, true, 48);
 assert(!h.warm(21450001) && !h.on());

 HeaterControl other;
 other.start(1);
 other.update(1000001, 20, 1000001, true, 48);
 assert(other.on()); // one channel's fault does not disable the other
 other.update(1000251, 20, 1000251, false, 48);
 assert(!other.on() && other.duty() == 0);

 // Simple thermal plant: exercise the closed loop and its duty limit under
 // two cooling loads. This validates control behavior, not board thermals.
 for (float ki : {0.01f, 0.002f}) {
 for (float cooling : {0.01f, 0.015f}) {
  HeaterControl plant;
  plant.start(1);
  float temperature = 25;
  for (uint64_t t = 1; t <= 300000001; t += 50000) {
   plant.update(t, temperature, t, true, 48, 0.08f, ki);
   assert(plant.fault() == HeaterControl::None);
   assert(plant.duty() >= 0 && plant.duty() <= 0.6f);
   temperature += 0.05f * (plant.duty() - cooling * (temperature - 25));
  }
  assert(fabsf(temperature - 48) < 0.5f);
  assert(plant.warm(300000001));
 }
 }
 puts("Heater control: freshness, fault latching, PWM, stability and thermal-model checks passed");
}
