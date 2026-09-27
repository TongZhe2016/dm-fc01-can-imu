# Early Bring-Up Record (Historical State)

This page archives the work on 2026-09-27 from the first flash through four-axis reference calibration, preserving the faults, assessments, commands, and follow-up plans recorded at the time. Each section reflects its recording time. See [VALIDATION.md](VALIDATION.md) for final test results and the [README](../../README.md) for current operating instructions.

Historical commands use paths from before the repository migration and are retained for traceability.

## Initial Implementation and Loss of USB Connectivity

Date: 2026-09-27. Software implementation and offline checks were complete at this stage. Loss of USB connectivity blocked hardware debugging; firmware and shared-bus motor validation awaited recovery.

### Results at This Stage

| Item | Evidence and result |
|---|---|
| Original board identification | DM-FC01, STM32H743 rev V, board ID 7140; original application hash `a4ad2fc346a543271cfc2c17a4baa21f14eb5f79`, PX4 1.16.1 |
| Parameter backup | 1019 used parameters; floating-point values came from MAVLink binary32, and integers were checked against NSH text saved at the same time. The MAVLink pseudo-parameter `_HASH_CHECK` was excluded; the final `WV_YRATE_MAX=90` was recovered from NSH. Backups are in the local artifact directory |
| Dedicated application build | ARM GCC 13.2.1, NuttX 12.12.0; final candidate board ID 7140, image_size 297196, maximum 1835008 bytes; linking and packaging succeeded |
| Latest candidate SHA256 | `f64931f64aeba30a67b796e1c4feec0815d5f7675f0e78abaf46a64cbd1000c9`; this candidate had not yet been flashed or validated on hardware |
| Software entry into bootloader over USB | All four development images completed erase, programming, and CRC verification, without pressing Boot or rewriting the bootloader. Bootloader: `PX4BLv1.16.1g99ad4703c9` |
| Sensor readings over USB | Third image: BMI088 accel FIFO 625 us/1600 Hz, gyro 500 us/2000 Hz; register/transfer/overflow/missed-DRDY counters all 0, initialization reset count 1; ICM45686 readings had previously worked |
| CAN filter defect identified | The first application entered a reboot loop; the hard-fault PC resolved to the vendor's `CanIface::configureFilters`. The original implementation incorrectly used a register offset ORed with a mask as a CPU address. It was changed to use the message RAM address, with mask, FIFO routing, and count fields also corrected |
| Subsequent CAN startup | The third application returned `CAN filter failed`, with no valid CAN IMU samples. INIT waiting was then bounded, and interrupt cleanup on stop was added; these changes still awaited hardware validation |
| Current hardware blocker | USB enumeration failed after the fourth application was flashed and verified. Host logs showed `device descriptor read/64, error -110` and `device not accepting address, error -71`. Resetting only the corresponding USB port did not recover it. This startup script did not autostart CAN, so the USB fault could not be attributed to the CAN filter on that basis |
| Offline protocol checks | 6 Python unittests passed: CRC reference, out-of-order/duplicate fragments, missing fragments/timeouts/CRC errors, sequence wraparound/restart/old sessions, missing samples/memory bounds, and clock drift/invalidation |
| C++ processing core | Passed with g++ ASan/UBSan: common windows across different ODRs, means, gap rejection, reversed timestamps, clipping, and cross-language CRC vectors |
| ROS 2 | `fc_clamp_can_imu` built successfully with colcon; ran for 5 seconds in isolated domain 173 and ended with SIGINT, with no error output. No real samples were available, so ROS data rate and latency were not validated |
| Motor precheck | Read-only inspect succeeded: four stationary motors with IDs 1–4, original parameters recorded. Both the earlier plan and the new locally bounded plan reported `live_motion_ready=true` |
| Motor simulation | 0.5 rpm, soft limits configured at precheck positions ±4°, with existing height/communication/fault protections retained. One set of 10 actions completed recording, release, and parameter restoration. The tuning score rejected the baseline because one simulated joint failed to reach 90% response; this was not a hardware bandwidth result |
| Test-tool fix | Failed fits now record inf/NaN as JSON null while retaining hard_fail; reports support null. The new unit test passed, and the simulation report could be regenerated |
| Hardware motor motion | **Not executed.** The user had confirmed site conditions and authorized low-speed tests within ±5° of each joint's current position. That authorization remained in effect; after recovery, current positions and device ownership required another read-only check |

### Local Evidence

Main directory: `/home/airman/FC-CLAMP_Real_Drone/.omx/artifacts/dm-fc01-imu/`.

- `preflash.json`, `factory-parameters.json`: original board, sensors, and parameters.
- `flash-01.log` through `flash-04.log`: four application-flashing records.
- `postflash-02.json`: hard-fault logs, sensor diagnostics, and USB diagnostics.
- `postflash-03.json`: 1600/2000 Hz readout status and CAN startup errors.
- `can-03/summary.json`: no IMU samples received in 30 seconds; not a passing validation record.
- `candidate.px4`, `candidate-manifest.json`: latest unvalidated candidate.
- `usb-rescue.px4`: recovery diagnostic image that autostarts only USB; not yet successfully flashed.
- `recovery-watch.log`: bounded recovery-program log while waiting for a physical reset to flash the diagnostic image.
- `ros-smoke.log`: ROS adapter process smoke check, without data.

Motor task record: `/home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/session.json`; configuration: `bounded.yaml`; simulation records: `simulate/*/summary.json`. The initial read-only hardware check is in the adjacent `imu-can-20260927-inspect` directory.

### Check Commands Executed at the Time

From the repository root:

```bash
PYTHONPATH=real_drone/can_imu python3 -m unittest discover -s real_drone/can_imu/tests -v
python3 -m compileall -q real_drone/can_imu real_drone/ros2_ws/src/fc_clamp_can_imu
bash -n real_drone/docs/dm-fc01/PX4-Autopilot_dm-fc01/ROMFS/can_imu/init.d/rcS
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ireal_drone/docs/dm-fc01/PX4-Autopilot_dm-fc01/src/modules/can_imu \
  real_drone/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
cmake --build real_drone/docs/dm-fc01/PX4-Autopilot_dm-fc01/build/damiao_dm-fc01_imu -j4
sh real_drone/aerial_arm_motor_autotune/agent.sh plan \
  --config /home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/bounded.yaml \
  --max-evaluations 1 --max-duration-s 300
sh real_drone/aerial_arm_motor_autotune/agent.sh baseline --simulate \
  --config /home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/bounded.yaml \
  --max-evaluations 1 --max-duration-s 300 \
  --output /home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/simulate
```

From `real_drone/aerial_arm_motor_autotune`:

```bash
PYTHONPATH=tests .venv/bin/python -m unittest test_trace_nonfinite -v
```

See the README for the ROS build command. After sourcing the install environment, the smoke check was:

```bash
ROS_DOMAIN_ID=173 timeout --signal=INT 5 ros2 run fc_clamp_can_imu can_imu_node
```

Exit code 124 came from the bounded timeout; the node reported no errors. Running the ROS adapter process alone did not establish IMU data reception or EKF validation.

### Recovery Plan at the Time

1. Catch the bootloader after an on-site RESET/full power cycle, flash `usb-rescue.px4`, and confirm stable USB operation.
2. Load parameters, one sensor, and high-rate FIFOs incrementally, then start CAN manually. Save USB status and host enumeration logs at each step to locate the USB fault.
3. Check extended-frame filtering, transmission/reception, CRC, synchronization, stop/restart, and boot sessions. Flash the autostart candidate only after achieving a complete, valid 200 Hz stream.
4. Run the board continuously for 30 minutes, recording valid-sample rate, missing samples, longest interruption, and latency. Do not use average frequency to hide interruptions. Record each heater's thermal tests and calibration separately.
5. Repeat read-only hardware checks and verify the ±5° range around current positions. Run two bounded motor batches, each at most 300 seconds: a baseline with the IMU primary stream disabled and a comparison with it enabled. Keep the same motion configuration and original parameters, recording all bus frames, error-counter deltas, motor replies, and IMU latency.
6. Use load bounds, motor timeouts, and feedback latency to determine whether 1 Mbit/s supports the current 150 Hz control loop and 200 Hz IMU stream. No bandwidth validation conclusion was available at this stage.

Other unvalidated items included six-face/per-axis orientation, extrinsics, gyroscope and accelerometer calibration, absolute synchronization accuracy, filter dynamic delay, standalone operation across USB disconnect/reconnect, congestion/bus-off recovery, temperature-control fault injection, and EE EKF integration. Exact ICM sampling-time reconstruction and dual-IMU fusion had not been implemented.

## Repository Migration Plan (State at the Time)

The user requested delivery of the related implementation to `git@github.com:TongZhe2016/dm-fc01-can-imu.git` after firmware work was complete, with the parent project referencing it as the `real_drone/dm-fc01-can-imu` submodule. An SSH query on 2026-09-27 succeeded; the remote had no published branches at the time of the query.

Migration scope included firmware and required build dependencies, host receiver/USB maintenance tools, ROS integration, tests, the README, protocol/calibration/build-and-recovery documentation, and validation records. The parent project would retain only the necessary integration entry points. ROS integration would no longer reference adjacent source through the existing directory layout. Vendor provenance and licenses would remain in the standalone repository. Generated logs, virtual environments, firmware binaries, and device-parameter backups would remain local artifacts.

Migration acceptance required a standalone checkout of the new repository, initialization of declared dependencies, rebuilding and running offline checks, and checking flashing/recovery tools and ROS entry points before pushing and registering the parent project's gitlink. Then remove `real_drone/docs/dm-fc01/PX4-Autopilot_dm-fc01`, `real_drone/can_px4`, their `.gitmodules` entries, and local submodule metadata from the parent project, along with duplicated migrated tools and documents. Two MAVLink backup helper scripts still depended on the old `can_px4` path; those dependencies had to be removed first.

This plan was recorded during USB recovery, before the old directories were deleted or the repository published.

## 2026-09-27 Retest After a Full-System Reboot

Rebooting the host restored USB. The diagnostic build ran BMI088 acceleration at 1600 Hz and gyroscope at 2000 Hz, with zero SPI errors, FIFO overflows, and missed DRDY events. After CAN startup, 946 samples were decoded successfully over approximately 4.73 seconds of continuous valid data, then output stopped. The 10-second window average was only 94.59 Hz, so this did not pass continuous 200 Hz output validation. Results were saved locally in `whole-reboot-can/summary.json`. Axis readings varied substantially during this run, so it was not a stationary-noise test.

The fifth flash completed and verified successfully, with autostart running. New register diagnostics showed PSR=0x77b (ACK error / error passive) and TXBRP=0xffffffff (all transmit slots pending). The standalone module had omitted driver-clock initialization, preventing transmission-timeout cleanup. `SystemClock::instance()` was added to the source, and the sixth version was built but still awaited flashing and validation. CANerr=0 had previously counted only abort/timeout events and could not establish the absence of physical bus errors.

During the sixth automatic reboot into the bootloader, the host's tegra-xusb again reported a transfer-event error and USB descriptor -110; writing had not begun. USB controller recovery and CAN power checks were in progress. Motor motion had not been executed.

The sixth version subsequently enumerated, flashed, and verified successfully after rebinding the host's `tegra-xusb` controller. The CAN driver-clock initialization fix took effect: timed-out transmissions were continuously cleared while disconnected, so the queue no longer remained permanently blocked. The CANerr increase was consistent with transmission timeouts. The user then confirmed that the flight controller CAN cable had been disconnected and reconnected. A 30-second recording after reconnection still received no IMU frames, and read-only motor inspect did not fully pass (CAN response timeout). An on-site CAN1 connector check was requested. Source and tools had been copied into the new standalone repository for migration builds; the old directories had not been deleted and the remote had not been published.

The migration directory passed a standalone build: `make -C real_drone/dm-fc01-can-imu damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/home/airman/FC-CLAMP_Real_Drone/.omx/venvs/can-imu-build/bin/python`. All 6 Python protocol tests and C++ ASan/UBSan core checks passed at the new path. The ROS colcon build at the new path passed. The node was rejected by the exclusive lock while the synchronization receiver was running; after stopping the receiver, the node ran alone for 5 seconds normally (timeout 124), but without IMU data. Delivery cleanup of the old directories and remote was still pending.

The maintained source was now in `real_drone/dm-fc01-can-imu`. The board ran the sixth version (clock fix); the new directory also renamed the status field from sent to queued to distinguish enqueueing from delivery. The firmware passed a full build in the new directory, but the field-renaming version had not been flashed again. The vendor manual specifies only one CAN1 interface, with pins 1 GND, 2 VBAT, 3 H, and 4 L. The earlier question about CAN2 did not apply to this board. No motor-motion experiments had been run; full validation, final publication, and old-submodule removal awaited restored connectivity.

## Hardware Incident: Large J4 Rotation and Cable Pull

The user reported that J4 had rotated through a large angle and pulled on the cable. The cable tension was then relieved, connectors checked, and the arm returned to [90,0,0,0], with readiness for calibration reconfirmed. No bounded motion batch had been executed, but that did not mean the hardware had remained motionless: earlier statements that motor motion had not been executed referred only to the planned motion experiments. The cause was undetermined. One issue to investigate was whether motors incorrectly accepted extended frames: the low bits of the old IMU IDs included 1–4, the vendor motor protocol specified only standard frames, and there was no empirical evidence that the motors rejected extended frames.

USB commands `can_imu stop`, `param set CI_AUTOSTART 0`, and `param save` were executed, confirming not running. Shared-bus IMU output with the old IDs must not resume until the issue is resolved. The four-motor check still timed out, zero references had not been written, and per-motor read-only diagnostics were in progress.

Per-axis read-only recheck: J1/J2/J3 UUIDs matched the original records, positions were approximately 1.6962/−3.6983/10.2374°, absolute speeds were all below 0.002 rpm, and currents were all zero. The J4 UUID query timed out. Four-axis calibration had not been performed. The source default for CI_AUTOSTART was changed to 0, and 0 was separately saved on the current board; existing parameters still override new firmware defaults.

## 2026-09-27 15:17 Four-Axis Reference Calibration Completed

The user confirmed repair of the J4 disconnection, a full-system power cycle, and placement at [90,0,0,0]. USB checks showed can_imu not running and CI_AUTOSTART=0 saved. All four axes matched their original UUIDs and passed stationarity checks. The shared driver's `calibrate_reference` function wrote the reference coordinates. Per-axis verification and a subsequent full query passed: 89.999443, 0.000387, 0.000222, and −0.000175°. No motion targets were sent and gains were unchanged. Calibration persistence had not yet been verified through another power cycle.

Command: `real_drone/aerial_arm_motor_autotune/.venv/bin/python /home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/calibrate_reference_run.py` (the same wrapper passed --simulate before execution). Results were saved as `reference-calibration-live-20260927_151708.json` in that directory. An initial attempt to read the tuning configuration with the driver CLI exited because the control field was missing, before opening the hardware. Calibration then completed using the validated tuning configuration and the same maintenance function as the UI. IMU CAN remained disabled; the cause of J4's unexpected motion and shared-bus protocol isolation still required investigation.
