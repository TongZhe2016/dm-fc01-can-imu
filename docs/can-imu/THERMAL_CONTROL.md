# IMU temperature control

The firmware controls the BMI088 and ICM45686 heaters independently. The validation board runs both at 50°C with tuned PI gains and saved autostart settings. Startup imports the saved parameters and starts heating with the `can_imu` module.

## Configuration

| Parameter | Source default | Validation board |
|---|---:|---:|
| `CI_AUTOSTART` | 0 | 1 |
| `CI_HEAT_EN` | 0 | 3 |
| `CI_HEAT_T` | 48°C | 50°C |
| `CI0_HEAT_P` / `CI0_HEAT_I` | 0.10 / 0.01 | 0.08 / 0.002 |
| `CI1_HEAT_P` / `CI1_HEAT_I` | 0.10 / 0.01 | 0.10 / 0.002 |
| `CI_EPOCH` | 1 | 3 |

`CI_HEAT_EN` is a bitmask: 1 enables BMI088, 2 enables ICM45686, and 3 enables both. `CI_HEAT_T` accepts 30–50°C. P is duty/°C and I is duty/(°C·s); accepted ranges are 0–1 and 0–0.1 respectively. Invalid or non-finite settings prevent module startup.

Parameters take effect when the module starts. Save changes with `param save`, then restart with `can_imu stop` and `can_imu start`. This briefly interrupts the CAN stream. Increment `CI_EPOCH` when changing the operating configuration. The saved board settings were verified through module restart and parameter readback; a full power-cycle check remains pending.

Heaters can only add heat. Choose a target above the board's unheated operating temperature within the supported range. ICM45686 approached 49°C with its heater off during testing, so the dual-channel tests used 50°C. Validate the target in the intended enclosure and airflow.

## Controller and feedback

Each channel uses PI control with anti-windup, a 50 ms PWM period, and a 60% commanded-duty limit. Outputs stay off for the first second while sensor data becomes available.

The internal `sensor_accel.timestamp_temperature` records the driver's temperature-read time. Repeated acceleration publications retain that timestamp. BMI088 reads its temperature registers nominally at 20 Hz; ICM45686 obtains temperature from its FIFO. The external CAN layout is version 1.

## Protection and readiness

Each enabled channel latches one of these faults after the startup grace period:

| Code | Fault condition |
|---|---|
| 0 | No fault |
| 1 | Temperature is NaN or infinite |
| 2 | Temperature timestamp is missing, in the future, or at least 200 ms old |
| 3 | Temperature is at least 60°C |
| 4 | At least five minutes have elapsed since module start and temperature is more than 5°C below target |

A fault turns off the affected channel until module restart. Investigate its cause before restarting. Module stop and destruction turn off both outputs. These protections depend on software execution; an MCU stall or failed switching device requires independent hardware protection.

The Warm flag requires fresh temperature within ±1°C of target for at least 10 consecutive seconds, with no heater fault. An observation gap of 200 ms resets the interval. A naturally warm sensor can satisfy this condition with its heater disabled.

Use `can_imu status` over USB to read the enable mask, target, PI gains, duty, Warm flags, and fault codes. CAN samples carry the selected sensor's temperature and Warm flag; the status packet includes both temperatures and a heater-fault bitmask. See [PROTOCOL.md](PROTOCOL.md) for the fields.

## Calibration and ROS

Calibrate at the operating temperature after both sensors reach thermal equilibrium. Temperature compensation uses a 48°C reference; the validation board still needs bias calibration and residual-drift measurements at 50°C.

Set ROS `require_warm` alongside the calibration requirement to gate `/ee_imu/data` on thermal readiness. `/ee_imu/raw` remains available during warm-up. Check temperature stability throughout calibration recordings.

## Verification

The [thermal validation record](THERMAL_VALIDATION.md) contains the firmware identities, measured curves and counters, tuning comparison, exact experiment commands, and local artifact paths. The tuned BMI088 peak was 50.38°C at a 50°C target, compared with 52.13°C using the original gains. A 280-second recording during heating received 56000 samples with zero missing samples or CAN errors.

From the repository root, run the controller checks with:

```bash
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/heater_test.cpp -o /tmp/can-imu-heater-test
/tmp/can-imu-heater-test
```

Tests cover startup grace, duty limits, stale/invalid timestamps, non-finite temperatures, overtemperature, fault latching, warm-up timeout, channel independence, disable behavior, and continuous readiness. Simplified thermal models exercise both original and tuned integral gains.

Hardware testing covers single-channel heating, dual-channel heating, sensor-loss shutdown, fault latching, and CAN continuity. Cold-start performance, environmental variation, independent heater-power measurements, and full power-cycle acceptance remain open. The 60°C cutoff and invalid-value cases were exercised offline.

## Vendor design

The vendor's DM-FCO1 Flight Controller User Manual V1.1 (2026-08-28), IMU sections and heater FAQ, specifies a BMI088 heater on PD14 and an ICM45686 heater on PD15. It states that the PX4 firmware enables heating by default. The development manual gives the same mapping. These references are in the parent project's `real_drone/docs/dm-fc01/` directory.

The retained vendor source at commit `b3d0fad488ac158d7af8cafb5de2b4ef8bc162b3` contains:

- `boards/damiao/dm-fc01/default.px4board`: enables `CONFIG_DRIVERS_HEATER`.
- `boards/damiao/dm-fc01/init/rc.board_defaults`: binds heater 1 to device ID 6946826 and heater 2 to 3407906, sets both targets to 48°C, and invokes `heater start`.
- `src/drivers/heater/heater.cpp` and `heater.h`: separate instances with temperature feedback, proportional/integral/feedforward control, and a 10 ms PWM period.

The board defaults and the original board's saved factory parameters agree:

| Setting | Heater 1, BMI088 | Heater 2, ICM45686 |
|---|---:|---:|
| Target | 48°C | 48°C |
| Proportional gain | 0.200 | 0.650 |
| Integral gain | 0.000 | 0.001 |
| Feedforward | 0.04 | 0.02 |

The old board-default script labels the second sensor “ICM42688” in a comment. The vendor manual and sensor startup script identify ICM45686. The current mapping follows those sources. The old gains are specific to that controller, which accumulates integral error per update; they must not be copied directly into the current controller, whose integral gain is applied per second.
