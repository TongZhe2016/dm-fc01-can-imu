# IMU thermal validation

Hardware tests on 2026-09-27 cover dual-channel temperature control, PI tuning, sensor-loss protection, and saved autostart configuration. See [Temperature control](THERMAL_CONTROL.md) for operation and parameter definitions.

## Hardware verification (2026-09-27)

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

### Temperature-loss fault injection

With both heaters selected, the BMI088 accelerometer driver and ICM45686 driver were stopped one at a time. Each affected channel reported stale-temperature fault code 2, zero commanded duty, and a cleared Warm flag within approximately 0.254 seconds as observed through USB polling. This includes command and polling latency and is not a precise GPIO shutdown-time measurement. The other channel remained fault-free. Restarting the sensor did not clear the fault; restarting the module cleared the latch. Both sensor drivers were restored afterward.

NaN/infinity, the 60°C cutoff, future timestamps, and the five-minute failure-to-warm condition were tested offline, not induced physically. GPIO voltage, actual heater power, a failed switching device, and MCU-stall protection were not measured. ICM reverse-timestamp counters still increase in diagnostic operation; this test does not validate its per-sample timing or fusion readiness.

### Session state and records

The final 30-second CAN check received 5999 samples with zero missing samples or CAN errors. BMI088 accelerometer/gyroscope reported zero bad transfers, FIFO overflows, and missed DRDY events; ICM reported zero bad transfers or FIFO overflows. This test session ended with `CI_AUTOSTART=1`, `CI_HEAT_EN=0`, and `CI_HEAT_T=48`, the primary stream running, and both heater duties at zero. The receiver processes exited normally.

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

## PI tuning (2026-09-27)

A subsequent firmware revision exposes `CI0_HEAT_P`, `CI0_HEAT_I`, `CI1_HEAT_P`, and `CI1_HEAT_I` as independently configurable gains. P is duty/°C and I is duty/(°C·s). Runtime validation rejects non-finite values, P outside 0–1, or I outside 0–0.1 before starting CAN or heating. Existing freshness, overtemperature, startup, duty-limit, and fault-latch protections remain active. Source defaults remain P=0.10 and I=0.01; the tuned board settings are recorded below.

The tuning package has SHA-256 `045fd96e9f4d1aa6a1e9e2b91496c6c8abbe11452fabba8eec204f6753df19ce`, image size 257260 bytes, and board ID 7140. Programming and verification passed. A recurrent host tegra-xusb enumeration failure required controller rebinding after reboot; NSH recovered and confirmed the new per-channel parameter support before heating began.

The candidate lowers integral gain to reduce heat accumulation during warm-up. A first 180-second dual-channel trial was followed by natural cooling with sensors running, a 120-second original-gain baseline, another cooling period, and a 300-second candidate repeat. All heating runs targeted 50°C with a 53°C test-abort threshold and a 60% commanded-duty cap. Each run disabled heating at its end.

| Channel | Original P | Original I | Selected P | Selected I |
|---|---:|---:|---:|---:|
| BMI088 | 0.10 | 0.01 | 0.08 | 0.002 |
| ICM45686 | 0.10 | 0.01 | 0.10 | 0.002 |

The gain selection uses a matched-start comparison. Cold starts and changes in ambient temperature or airflow require further testing.

### Measured comparison and selected settings

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

Selected board parameters were saved as `CI0_HEAT_P=0.08`, `CI0_HEAT_I=0.002`, `CI1_HEAT_P=0.10`, `CI1_HEAT_I=0.002`, and `CI_HEAT_T=50`. `CI_EPOCH` was incremented to 2 for the configuration change. The tuning session ended with heating disabled (`CI_HEAT_EN=0`), `CI_AUTOSTART=1`, and the 200 Hz primary stream active. Temperature compensation is still referenced to 48°C and calibration remains invalid: operation at 50°C requires calibration at that operating temperature, before marking calibration valid.

Tuning artifacts are in the parent project's `.omx/artifacts/dm-fc01-imu/thermal-tune-20260927/`, including `source-manifest.json`, `source.patch`, `flash.log`, `baseline-result.json`, `candidate-a-result.json`, `candidate-repeat-result.json`, `comparison.json`, `pi-comparison.png`, `can-repeat/summary.json`, and `final-parameters.json`. The bounded runner holds the shared hardware lock, checks temperature and fault status, and disables heating in its cleanup path. The initial USB-console startup timeout occurred before heater configuration and is recorded separately as `usb-startup-failure.json`; subsequent readback confirmed both heaters disabled before the first trial.

Executed tuning commands from the parent project root:

```bash
python3 .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/tune_run.py --mask 3 --target 50 --duration 180 --name candidate-a --p0 .08 --i0 .002 --p1 .1 --i1 .002
python3 .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/tune_run.py --mask 3 --target 50 --duration 120 --name baseline --p0 .1 --i0 .01 --p1 .1 --i1 .01
python3 .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/tune_run.py --mask 3 --target 50 --duration 300 --name candidate-repeat --p0 .08 --i0 .002 --p1 .1 --i1 .002
python3 real_drone/dm-fc01-can-imu/host/can_imu/receive.py --interface can0 --duration 280 --sync --output .omx/artifacts/dm-fc01-imu/thermal-tune-20260927/can-repeat
```

## Saved heater autostart (2026-09-27)

The validation board has persistent settings `CI_AUTOSTART=1`, `CI_HEAT_EN=3`, `CI_HEAT_T=50`, BMI088 P/I=0.08/0.002, and ICM45686 P/I=0.10/0.002. `CI_EPOCH` was advanced to 3. Parameters were saved with `param save`, then the `can_imu` module was restarted. Both channels reached Warm during a 90-second observation without heater faults. A concurrent 60-second CAN recording received 12000 samples with zero missing samples, socket drops, or CAN errors. The recording and saved parameter readback are in the parent project's `.omx/artifacts/dm-fc01-imu/thermal-autostart-20260927/`. The exact command sequence is recorded in `usb.jsonl`; `result.json` contains temperature observations and parameter readback.

The board was left streaming with both heaters enabled at 50°C. The source defaults still disable heating for an unconfigured board. This operation verified module restart and saved configuration; a complete power-cycle test was not performed. The existing startup script imports saved parameters and starts the module when `CI_AUTOSTART=1`, so the saved configuration enables temperature control on subsequent boots. Earlier sections retain the settings at the end of each test session.

## Source verification

The documentation and controller cleanup passed the following checks from the standalone repository root:

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/heater_test.cpp -o /tmp/can-imu-heater-test
/tmp/can-imu-heater-test
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
cmake --build build/imu-thermal-check --parallel 4
git diff --check
```

All 7 protocol tests and both C++ sanitizer checks passed. The firmware build completed. The cleanup build was not flashed; the board continues running the tested tuning image identified above.
