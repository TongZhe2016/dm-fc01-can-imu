# IMU Temperature Control

Both IMUs have dedicated heaters. The standalone firmware provides two independent PI control loops with a shared default target of 48°C. Heating reduces temperature variation; bias calibration at the operating temperature and measurement of residual drift are still required.

## Vendor Design and Previous Firmware

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

## Standalone Controller

`CI_HEAT_EN` selects the channels: 0 disables both, 1 enables BMI088, 2 enables ICM45686, and 3 enables both. `CI_HEAT_T` sets the common target, default 48°C and accepted range 30–50°C. Parameters take effect after module restart. The last saved release-parameter record has `CI_HEAT_EN=0`; this is a historical readback, not a fresh device query.

Each channel has configurable proportional and integral gains (`CI0_HEAT_P/I` for BMI088 and `CI1_HEAT_P/I` for ICM45686), with source defaults 0.10/°C and 0.01/(°C·s). Gains take effect after module restart; finite P values in 0–1 and I values in 0–0.1 are accepted. The controller uses anti-windup, a 50 ms PWM period, and a maximum 60% commanded duty cycle. A one-second startup grace period keeps the outputs off while sensor data becomes available. The heaters provide heat only: the target must be attainable above the board's unheated operating temperature in the intended enclosure and airflow. A 48°C target is inherited from the vendor configuration and still needs thermal validation for the deployment conditions.

The revised implementation carries `timestamp_temperature` through the internal `sensor_accel` topic. This is the driver temperature-read time; repeated acceleration publications do not refresh it. BMI088 temperature-register reads now run nominally at 20 Hz instead of 1 Hz. ICM45686 obtains temperature from its FIFO. The external CAN packet layout remains version 1.

This fixes a freshness gap in the earlier implementation: checking the acceleration publication timestamp could treat an old cached temperature as fresh while acceleration continued to arrive.

## Protection and Readiness

Each enabled channel latches a fault after the startup grace period if:

- Temperature is NaN or infinite.
- The temperature-read timestamp is missing, in the future, or at least 200 ms old.
- Temperature reaches or exceeds 60°C.
- Five minutes have elapsed since module start and temperature remains more than 5°C below target. This check also remains active afterward.

A fault disables that channel. It remains latched until module restart; investigate the cause before restarting. Module stop and destruction turn both outputs off. These are software protections serviced by the module; they do not establish a hardware cutoff for a stalled MCU or failed switching device.

The Warm flag requires fresh temperature within ±1°C of target continuously for at least 10 seconds, with no heater fault. An observation gap of at least 200 ms resets this interval. A naturally warm sensor can satisfy the flag with heating disabled. `can_imu status` reports the enable mask, target, per-channel duty percentage, Warm state, and fault codes:

| Code | Meaning |
|---|---|
| 0 | No fault |
| 1 | Invalid temperature |
| 2 | Stale or invalid temperature timestamp |
| 3 | Overtemperature |
| 4 | Warm-up timeout / target not maintained |

For operational ROS use, enable `require_warm` together with the existing calibration requirement so `/ee_imu/data` waits for thermal readiness. `/ee_imu/raw` remains available for diagnostics. Calibrate after thermal stabilization, and verify that temperature remains stable during the recording.

## Offline Verification

From the standalone repository root:

```bash
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/heater_test.cpp -o /tmp/can-imu-heater-test
/tmp/can-imu-heater-test
```

The test covers startup grace, PWM duty limits, stale cached temperatures, invalid/future timestamps, NaN/infinity, the 60°C cutoff, fault latching, warm-up timeout, per-channel independence, disable behavior, and continuous thermal readiness. Two simplified thermal loads exercise closed-loop convergence. These models are software checks, not measured board thermal characteristics.

### Verification Record (2026-09-27)

The heater test above passed with ASan/UBSan. The following commands also passed from the standalone repository root:

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
make damiao_dm-fc01_imu BUILD_DIR=build/imu-thermal-check JOBS=4 \
  PYTHON_EXECUTABLE=/home/airman/FC-CLAMP_Real_Drone/.omx/venvs/can-imu-build/bin/python
git diff --check
```

Results: all 7 Python protocol tests passed; the C++ processing-core checks passed with ASan/UBSan; the full firmware build succeeded. The local output is `build/imu-thermal-check/damiao_dm-fc01_imu.px4`. The Python path is specific to the validation host. No flashing or live heating was performed in this verification.

## Hardware Acceptance Before Deployment

With separately authorized hardware execution, first validate one heater at a time, then both together. Record both temperatures, commanded duty, fault state, Warm state, and IMU stream counters from cold start through steady operation. Confirm channel mapping and measure warm-up time, overshoot, steady-state error, and recovery under expected airflow and ambient conditions. Verify the 60% duty cap leaves enough heating margin without changing it merely to suppress a warm-up fault.

Validate stale/invalid-temperature protection through controlled fault injection and confirm that module stop disables both outputs. Check continued 200 Hz sampling and CAN delivery with the added temperature reads. Repeat bias and noise measurements after reaching thermal equilibrium, then perform calibration at the chosen operating temperature. Persist heater autostart configuration only after these checks pass.

The revised controller passed the bounded hardware tests below. Heating remains disabled by default and was disabled again after testing.


## Hardware Verification (2026-09-27)

USB enumeration initially failed with errors -110/-71. Rebinding the Jetson `3610000.usb` tegra-xusb controller restored the DM-FC01 USB console. The shared hardware lock was held for flashing and each control experiment. Application programming and verification succeeded; bootloader firmware was unchanged.

The flashed package SHA-256 is `436f9d45f5fedd941fcd24a73c479165b1784993eb63febcfcd6aef3c02c6442`, board ID 7140, application image size 256788 bytes. USB reports base commit `16c319159e93cc977081d4469b6015dde2205e33` and build time Sep 27 2026 20:25:10; the image includes the uncommitted thermal changes captured in the local source manifest and patch, so the base hash alone does not identify its source.

| Test | Duration limit | Target | Selected sensor range in final 60 seconds |
|---|---:|---:|---|
| BMI088 heater only | 180 s | 48°C | BMI088: 47.63–48.38°C |
| ICM45686 heater only | 120 s | 50°C | ICM45686: 49.66–50.42°C |
| Both heaters | 300 s | 50°C | BMI088: 49.50–50.25°C; ICM45686: 49.82–50.54°C |

All runs completed without heater faults. Both Warm flags were set at the end of the dual-channel test. BMI088 peaked at 49.38°C in its 48°C single-channel test and 52.00°C in the 50°C dual-channel test; the latter shows approximately 2°C startup overshoot. ICM45686 peaked at 50.62°C during the dual-channel test. These are sampled USB readings, not independent surface-temperature measurements. Each run enforced a 53°C software test-abort threshold and disabled both heaters afterward.

Before heating, ICM45686 was already near 48°C and subsequently reached about 49°C with its own heater off. Therefore 50°C was used for its individual and the dual-channel tests. This establishes operation in the observed warm-start environment; cold-start performance and different ambient/airflow conditions remain unmeasured.

During BMI088 heating, a 160-second CAN recording received 32000 samples at 199.994 Hz. During dual heating, a 275-second recording received 54999 samples at 199.995 Hz. Both had zero missing samples, socket drops, CAN errors, FIFO publication losses, source gaps, and incomplete windows in the recorded module session.

### Temperature-Loss Fault Injection

With both heaters selected, the BMI088 accelerometer driver and ICM45686 driver were stopped one at a time. Each affected channel reported stale-temperature fault code 2, zero commanded duty, and a cleared Warm flag within approximately 0.254 seconds as observed through USB polling. This includes command and polling latency and is not a precise GPIO shutdown-time measurement. The other channel remained fault-free. Restarting the sensor did not clear the fault; restarting the module cleared the latch. Both sensor drivers were restored afterward.

NaN/infinity, the 60°C cutoff, future timestamps, and the five-minute failure-to-warm condition were tested offline, not induced physically. GPIO voltage, actual heater power, a failed switching device, and MCU-stall protection were not measured. ICM reverse-timestamp counters still increase in diagnostic operation; this test does not validate its per-sample timing or fusion readiness.

### Final State and Records

The final 30-second CAN check received 5999 samples with zero missing samples or CAN errors. BMI088 accelerometer/gyroscope reported zero bad transfers, FIFO overflows, and missed DRDY events; ICM reported zero bad transfers or FIFO overflows. Final saved parameters are `CI_AUTOSTART=1`, `CI_HEAT_EN=0`, and `CI_HEAT_T=48`; the primary stream is running, and both heater duties are zero. Test receiver processes have exited. No motor commands were sent.

Artifacts are in the parent project's `.omx/artifacts/dm-fc01-imu/thermal-live-20260927/`: `preflash.json`, `flash.log`, `postflash.json`, `source-manifest.json`, `source.patch`, `heat-*-result.json`, `thermal-summary.json`, `thermal-results.png`, `fault-injection-result.json`, `final-usb.json`, and `can-*/summary.json`. Local bounded runners are `thermal_run.py` and `fault_test.py`.

Commands executed from the parent project root (output paths are in the artifact directory above):

```bash
python3 real_drone/dm-fc01-can-imu/host/can_imu/flash.py real_drone/dm-fc01-can-imu/build/imu-thermal-check/damiao_dm-fc01_imu.px4 --wait 30
python3 .omx/artifacts/dm-fc01-imu/thermal-live-20260927/thermal_run.py --mask 1 --duration 180
python3 .omx/artifacts/dm-fc01-imu/thermal-live-20260927/thermal_run.py --mask 2 --target 50 --duration 120
python3 .omx/artifacts/dm-fc01-imu/thermal-live-20260927/thermal_run.py --mask 3 --target 50 --duration 300
python3 .omx/artifacts/dm-fc01-imu/thermal-live-20260927/fault_test.py
```

CAN recording used `host/can_imu/receive.py --interface can0 --duration SECONDS --sync --output DIRECTORY` with durations 160, 275, and 30 seconds and respective directories `can-heat-1`, `can-heat-3`, and `can-final` under the artifact directory. Further deployment work is residual bias calibration at the selected temperature, cold-start/environmental testing, and gain adjustment if lower overshoot is required.


## Per-Channel PI Tuning (2026-09-27)

A subsequent firmware revision exposes `CI0_HEAT_P`, `CI0_HEAT_I`, `CI1_HEAT_P`, and `CI1_HEAT_I` as independently configurable gains. P is duty/°C and I is duty/(°C·s). Runtime validation rejects non-finite values, P outside 0–1, or I outside 0–0.1 before starting CAN or heating. Existing freshness, overtemperature, startup, duty-limit, and fault-latch protections remain active. Source defaults remain P=0.10 and I=0.01; the tuned board settings are recorded below.

The tuning package has SHA-256 `045fd96e9f4d1aa6a1e9e2b91496c6c8abbe11452fabba8eec204f6753df19ce`, image size 257260 bytes, and board ID 7140. Programming and verification passed. A recurrent host tegra-xusb enumeration failure required controller rebinding after reboot; NSH recovered and confirmed the new per-channel parameter support before heating began.

The candidate lowers integral gain to reduce heat accumulation during warm-up. A first 180-second dual-channel trial was followed by natural cooling with sensors running, a 120-second original-gain baseline, another cooling period, and a 300-second candidate repeat. No sensor shutdown was used for cooling. All heating runs targeted 50°C with a 53°C test-abort threshold and a 60% commanded-duty cap. Each run disabled heating at its end.

| Channel | Original P | Original I | Selected P | Selected I |
|---|---:|---:|---:|---:|
| BMI088 | 0.10 | 0.01 | 0.08 | 0.002 |
| ICM45686 | 0.10 | 0.01 | 0.10 | 0.002 |

This is bounded empirical tuning with a matched-start comparison, not a globally optimal gain search or an independently identified thermal model. The deployment environment and colder starts still require validation. Integral gains from the vendor's controller use different update semantics and were not copied into this controller.


### Measured Comparison and Selected Settings

The original-gain baseline started at BMI088 45.50°C / ICM45686 49.09°C; the candidate repeat started at 45.50°C / 49.07°C. The temperature target was 50°C in both runs.

| Metric | Original BMI088 | Tuned BMI088 | Original ICM45686 | Tuned ICM45686 |
|---|---:|---:|---:|---:|
| Observed peak temperature | 52.13°C | 50.38°C | 50.66°C | 50.66°C |
| First observed Warm flag | 21.03 s | 16.63 s | 13.32 s | 13.32 s |
| Final 60 s minimum | 49.63°C | 49.75°C | 49.46°C | 49.52°C |
| Final 60 s maximum | 50.38°C | 50.38°C | 50.66°C | 50.66°C |
| Final 60 s RMS error relative to target | 0.202°C | 0.161°C | 0.317°C | 0.211°C |

The baseline lasted 120 seconds and the candidate repeat 300 seconds, so their final-minute windows occur at different elapsed times. Peak and readiness metrics include each entire run; these are sampled USB measurements. BMI088 startup overshoot decreased from 2.13°C to 0.38°C, about 82%. ICM's maximum excursion did not improve in the repeat, although its final-minute RMS error decreased. A separate 180-second candidate trial from 44.00°C / 47.64°C peaked at 50.50°C / 50.42°C. These bounded results support selecting the new gains but do not establish performance across all ambient temperatures.

The 280-second CAN recording during the candidate repeat received 56000 samples at 199.997 Hz, with zero missing samples, socket drops, CAN errors, lost FIFO publications, source gaps, and incomplete windows. All three tuning runs completed without heater faults. ASan/UBSan heater tests passed with both original and reduced integral gains in the simplified thermal models; all 7 Python protocol tests passed and the firmware build completed.

Selected board parameters were saved as `CI0_HEAT_P=0.08`, `CI0_HEAT_I=0.002`, `CI1_HEAT_P=0.10`, `CI1_HEAT_I=0.002`, and `CI_HEAT_T=50`. `CI_EPOCH` was incremented to 2 for the configuration change. Heating was disabled after testing (`CI_HEAT_EN=0`), while `CI_AUTOSTART=1` and the 200 Hz primary stream remain active. The selected gains are persistent board settings; source gain defaults remain 0.10/0.01. Temperature compensation is still referenced to 48°C and calibration remains invalid: operation at 50°C requires calibration at that operating temperature, rather than merely setting the calibrated flags.

Tuning artifacts are in the parent project's `.omx/artifacts/dm-fc01-imu/thermal-tune-20260927/`, including `source-manifest.json`, `source.patch`, `flash.log`, `baseline-result.json`, `candidate-a-result.json`, `candidate-repeat-result.json`, `comparison.json`, `pi-comparison.png`, `can-repeat/summary.json`, and `final-parameters.json`. The bounded runner holds the shared hardware lock, checks temperature and fault status, and disables heating in its cleanup path. The initial USB-console startup timeout occurred before heater configuration and is recorded separately as `usb-startup-failure.json`; subsequent readback confirmed both heaters disabled before the first trial.

Executed tuning commands from the parent project root:

```bash
python3 .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/tune_run.py --mask 3 --target 50 --duration 180 --name candidate-a --p0 .08 --i0 .002 --p1 .1 --i1 .002
python3 .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/tune_run.py --mask 3 --target 50 --duration 120 --name baseline --p0 .1 --i0 .01 --p1 .1 --i1 .01
python3 .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/tune_run.py --mask 3 --target 50 --duration 300 --name candidate-repeat --p0 .08 --i0 .002 --p1 .1 --i1 .002
python3 real_drone/dm-fc01-can-imu/host/can_imu/receive.py --interface can0 --duration 280 --sync --output .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/can-repeat
```
