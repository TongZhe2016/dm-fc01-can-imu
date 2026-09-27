# DM-FC01 CAN IMU Hardware Validation

Test date: 2026-09-27. Hardware: STM32H743VI rev V, BMI088 + ICM45686, CAN1 classic CAN at 1 Mbps, and four ENCOS motors with IDs 1–4. Host: Jetson with native SocketCAN can0. Firmware target: `damiao_dm-fc01_imu`, board ID 7140.

## Validation Overview

- USB application reboot into the bootloader, erase, write, and verification succeeded. The bootloader was unchanged.
- BMI088 accelerometer FIFO ran at 1600 Hz and gyroscope FIFO at 2000 Hz, with the primary stream resampled to 200 Hz. During the shared-bus comparison, there were no SPI transfer errors, overflows, missed DRDY events, or gaps in the valid stream. See “Diagnostic Stress and Scheduling Fix” for diagnostic stress testing.
- Standard-frame status output and a 35-second full stream both passed joint-stationarity monitoring: 7009 samples, 199.99965 Hz within the stream, all flags=1, no missing samples, and maximum joint drift below 0.004°. The 40-second observation window included explicit startup/shutdown silence; its 175.21 Hz window average is not the stream frequency.
- The four-axis reference pose [90,0,0,0] was written and read back. All four identities were verified after repairing the J4 disconnection.
- Two identical 10-action low-speed baseline runs both reached complete: 0.5 rpm, 0.5 seconds per segment, and 150 Hz control. Each run included approximately 30 seconds of motion and 34 seconds of bus observation. The original speed-loop gains KP=0.006 and KI=0.04999 were preserved, restoration was confirmed afterward, and all four motor currents were zero.

## Shared CAN Bus Comparison

| Metric | IMU primary stream disabled | 200 Hz IMU primary stream |
|---|---:|---:|
| Estimated bus-utilization lower bound | 18.67% | 35.99% |
| Conservative bit-stuffing upper bound | 22.59% | 43.66% |
| Motor feedback latency P50 | 3.3185 ms | 3.5140 ms |
| Motor feedback latency P99 | 4.881 ms | 5.203 ms |
| Maximum motor feedback latency | 11.247 ms | 10.946 ms |
| Motor feedback records | 16440 | 15864 |
| Stale feedback / CAN errors / safety events | 0 / 0 / 0 | 0 / 0 / 0 |
| IMU samples | — | 6805 |
| IMU reception frequency | — | 199.9919 Hz |
| Missing IMU samples / socket drops | — / 0 | 0 / 0 |
| IMU arrival interval P99 / maximum | — | 6.479 / 6.864 ms |
| IMU sample age after clock mapping, P99 / maximum | — | 6.580 / 6.965 ms |

Utilization is calculated from observed standard-frame lengths and CAN bit-stuffing bounds, including independent synchronization requests; it was not measured with an oscilloscope. With the primary stream disabled, 1 Hz status and 10 Hz synchronization remain active. Feedback latency runs from the start of the control cycle to the reception time recorded by the driver, including sequential queries and host scheduling; it is not the physical transmission time of one frame. Maximum feedback latency exceeded one 6.67 ms cycle in both runs. No stale feedback occurred, but this does not establish a hard real-time deadline guarantee.

Conclusion: **1 Mbps provides sufficient bandwidth headroom for the tested four-motor 150 Hz control loop plus a 200 Hz IMU stream.** Higher motor rates, additional nodes, and different frame formats require retesting. This experiment validates communication capacity on the shared bus.

Maximum angles relative to the reference pose across the two runs were approximately J1=3.58°, J2=3.45°, J3=4.79°, and J4=2.27°. The test range was also constrained by the configured ±4° limits around measured starting positions and existing height, lost-link, current, and temperature protections.

## Diagnostic Stress and Scheduling Fix

The first continuous recording stopped after 1111.8 seconds with 222362 samples and 1 missing sample. The gap coincided with `top once` at device time 1314.945 seconds. Onboard counters increased by 6 lost FIFO updates, 1 incomplete window, and 1/2 gaps for the two sources; CAN errors and socket drops remained zero. This run did not pass the zero-missing-sample criterion.

The module's original priority of 235 was below top's 237. It was raised to 245, still below the SPI work queues at 250/253. A subsequent 120-second recording included 30 `top once` calls and received 23999 samples, with no missing samples, lost FIFO updates, source gaps, CAN errors, or socket drops. The sample interval P99 was 6.148 ms and the maximum was 10.890 ms. The long-duration result after the fix follows.

## 30-Minute Continuous Recording

The release version recorded continuously for 1800.002 seconds: 359995 samples at 199.9970 Hz, with 0 sequence-detected missing samples, 0 socket drops, 0 CAN errors, 0 lost FIFO updates, and 0 gaps for either source. The incomplete counter remained at its initial value of 10. Arrival interval P99 was 6.286 ms, maximum 13.125 ms; sample age after clock mapping had P99 6.665 ms and maximum 14.129 ms. The run included 5 `top once` calls.

This run was originally planned as passive endurance testing. When the user requested a pause partway through, a conversation interruption prevented the stop operation from executing. The receiver/IMU synchronization process continued until the scheduled 30-minute end, covering a period when the user operated the motors independently. The recording observed 164250 frames for each motor ID and 8 frames with ID 0x7FF; the recording program sent no motor-control commands. This is an IMU continuity record under mixed activity, not a fixed motor-control-load experiment or a stationary-noise measurement. The bandwidth conclusion uses the two controlled comparison runs above. This failure to execute the pause was explained to the user.

With both sensors and 200 Hz CAN running, one `top once` snapshot showed CPU idle at 59.11%, the IMU module at approximately 4.96%, SPI1 at approximately 13.77%, and SPI4 at approximately 21.62%. This was not a worst-case execution-time measurement.

## ROS Reception

The ROS package passed a standalone build and startup check without data. Live-stream testing found and fixed a Linux SocketCAN mask issue: including CAN_ERR_FLAG assigned the socket to the error-frame receive list. CLI and ROS now share the standard-frame mask `0xC00007F8`. Filtered reception received 2000 samples in 10 seconds with no missing samples.

With the default ROS calibration gate, a 20-second run received raw=3986 and data=0, with status calibration_required and correct message metadata. A transport-only retest explicitly setting `require_calibration:=false` received raw=3929 and data=3928 in 20 seconds, with no missing samples in the node's decoded stream and status ready. Subscriber-side sample age had P99 8.183 ms and maximum 9.051 ms.

DDS uses best-effort delivery. Subscriber counts depend on discovery timing and delivery and cannot be treated as CAN missing-sample counts.

## Follow-Up Check After Resumption

After the user finished independent debugging and authorized continuation, a further 60.000-second reception/synchronization-only check received 11999 samples at 199.9827 Hz, with no sequence gaps, socket drops, CAN errors, or lost FIFO updates. The incomplete counter remained at 10. Sample age had P99 6.220 ms and maximum 10.359 ms. USB readback confirmed the same version; neither BMI088 accelerometer nor gyroscope showed bad transfers, FIFO overflows, or missed DRDY events. The recording/synchronization process exited normally after the test, and the board retained the autostart primary stream.

## Firmware Version and Counter Baseline

The flashed release firmware was commit `ccb8a2ceb839e991c985c3d62bfbfed263eebca6`, with `.px4` SHA-256 `771b89faef9085256b17f0a5186c3e3c6e985dfd87aa7bf2dd45e3a9cbe22dea`. `CI_AUTOSTART=1` was persisted, and the application started automatically after flashing and rebooting. USB version and parameter readback matched. The first read after autostart showed incomplete=10; the startup transient was not fully captured. Subsequent recordings measured increments from this baseline; boot=27. ICM polling-time fallback remains, so ICM continues to serve diagnostic use.

## Driver Fixes and Hardware Incident

The vendor H7 CAN filter-address error and initialization-wait issue were fixed. CAN shutdown lifecycle handling and standalone driver-clock initialization were added, allowing transmission deadlines to take effect when disconnected. The shared motor driver gained ENCOS ID-range filtering; previously, it misreported IMU standard frame 0x6A0 as a “motor 1696 fault.” Regression tests confirmed that other-device frames are ignored while genuine motor faults still trigger a stop.

J4 once rotated unexpectedly and pulled a cable apart (the user initially identified J3, then corrected it). The motion was not observed, and its cause remains undetermined; it cannot be attributed to IMU extended frames. The old extended-ID test stream was disabled. The current standard frames passed the stationary and bounded-motion shared-bus tests above. See [BRINGUP_HISTORY.md](BRINGUP_HISTORY.md) for the early debugging history.

## Remaining Validation

Pending work: physical six-face/per-axis orientation and motion-capture extrinsics validation; full accelerometer/gyroscope calibration; absolute clock-synchronization accuracy; filter dynamic-delay measurements; heater cold-start and environmental testing; exact ICM FIFO timing reconstruction and dual-IMU fusion; and EE EKF integration. The primary stream remains marked uncalibrated, so the default ROS `/ee_imu/data` topic is calibration-gated; `/ee_imu/raw` provides diagnostic data. Output preserves the actual mounting tilt and gravity response.

## Reproducible Checks

Run from the standalone repository root:

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
python3 -m compileall -q host/can_imu ros2/fc_clamp_can_imu
bash -n ROMFS/can_imu/init.d/rcS
make damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/path/to/build-env/bin/python
```

Results: all 7 protocol tests passed, C++ ASan/UBSan core checks passed, and the standalone firmware passed a full build. After rebuilding the shared motor driver, all 9 native transport tests passed, including the regression for false faults from shared-bus frames. The simulated motion workflow completed; its low-speed response could not be used for tuning scores. Both hardware baseline runs completed.

## Local Records

Raw logs, firmware, and device parameters are stored on the validation host:

- `.omx/artifacts/dm-fc01-imu/standard-stream-guarded/`, `standard-endurance-30min/` (before the fix), `priority-diagnostic-stress/`, `release-endurance-30min/` (after the fix), and `resumed-final-check/`.
- In the same directory: `flash-release.log`, `release-ccb8a2ceb8.px4`, `release-ccb8a2ceb8.manifest.json`, `release-parameters.json`, `ros-gated-result.json`, and `ros-transport-result.json`.
- `~/.local/state/aerial-arm-motor-autotune/imu-can-20260927/motor-off-20260927_153000/`.
- In the same directory: `motor-on-20260927_153139/`, `can-comparison.json`, `reference-calibration-live-20260927_151708.json`, and `session.json`.

Motion used the parent project's `agent.sh baseline --live --fixture-ready --anchors ... --config bounded-repaired.yaml --max-evaluations 1 --max-duration-s 300 --log-frames`; the exact command is stored in each run's `command.json`. Recording used `host/can_imu/receive.py --sync --observe-bus`. Global cumulative RX dropped counts are not evidence of drops in a particular run; use recording-socket overflow and sequence-gap counters.

## Subsequent Thermal Firmware Verification

The revised thermal firmware was flashed and passed bounded single-heater, dual-heater, and sensor-loss protection tests on 2026-09-27. See [THERMAL_VALIDATION.md](THERMAL_VALIDATION.md) for image identities, tuning results, fault-injection checks, and saved heater autostart configuration. These results supplement the earlier release record above.
