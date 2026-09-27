# DM-FC01 CAN IMU Architecture and Implementation

The firmware consists of sensor drivers, six-axis processing, CAN transport, and host receiver tools. See [VALIDATION.md](VALIDATION.md) for hardware results and the [README](../../README.md) for usage and build instructions.

## Hardware and Platform

The MCU is an STM32H743VIH6. The firmware retains the vendor's PX4/NuttX board support, based on commit `b3d0fad488ac158d7af8cafb5de2b4ef8bc162b3`. The build target is `damiao_dm-fc01_imu`; the board runs sampling, CAN output, and USB maintenance tasks.

Hardware details were checked against the vendor's DM-FCO1 Flight Controller User Manual V1.1 (2026-08-28), development manual, pin table, and CubeMX project, and compared with the board-level source in this repository.

| Component | Wiring | Purpose |
|---|---|---|
| BMI088 accelerometer | SPI1, CS PA2, DRDY PA0 | Primary acceleration stream |
| BMI088 gyroscope | SPI1, CS PA3, DRDY PA1 | Primary angular-velocity stream |
| ICM45686 | SPI4, CS PC13, DRDY PE4 | Independent diagnostic source |
| CAN1 | RX PD0, TX PD1 | Shared-bus data and clock synchronization over classic CAN at 1 Mbps |
| USB | PA11/PA12, VBUS PA15 | NSH maintenance, flashing, and diagnostics |
| Heaters | BMI088 PD14, ICM45686 PD15 | Optional temperature control, disabled by default |

CAN1 is the only CAN interface. Pins 1/2/3/4 of the 4-pin SH1.0 connector are GND/VBAT/CAN_H/CAN_L; pin 2 carries battery voltage. Another ICM entry in the pin table lists PA4/PE2, conflicting with the PDF and board-level code. The current implementation uses PC13/PE4 and has successfully read the physical sensor.

The vendor startup script uses `-R 4` (YAW180), while some descriptive text says PITCH180. The current implementation retains the fixed driver rotation used in hardware readings. Verify IMU-to-motion-capture extrinsics physically, one axis at a time. Output preserves the actual mounting tilt.

## Onboard Application

`ROMFS/can_imu/init.d/rcS` starts USB, loads parameters, configures high-rate FIFOs, and starts both IMUs. It starts the CAN module when `CI_AUTOSTART=1`. The initial default is 0; enable and save it after checking IDs and wiring.

`src/modules/can_imu/CanImu.cpp` independently consumes accelerometer and gyroscope FIFOs and identifies sensor types by device_id. The uORB queue depth is 16, and lost updates are counted.

Processing chain: raw integers multiplied by the driver scale → subtraction of per-axis bias and temperature-slope compensation → per-axis scaling → two first-order low-pass stages → window-integrated averaging on a common 5 ms time grid. Output is acceleration specific force (m/s²) and angular velocity (rad/s), preserving the gravity response in the sensor coordinate frame.

Each low-pass stage defaults to 60 Hz, giving a combined −3 dB frequency of approximately 38.6 Hz and low-frequency group delay of approximately 5.3 ms. The 5 ms window adds 2.5 ms of window-center delay. Timestamps mark the window end; EKF time alignment must account for both filtering and window delays.

When a gap exceeds three raw sample periods, the module discards windows with incomplete coverage. Samples with out-of-order timestamps are ignored and counted. Amplitude clipping and polling/unknown timestamps are reported through quality flags.

## Timing and Dual IMUs

The 64-bit timestamp comes from the MCU's monotonic microsecond clock since boot, which restarts on each power-up. Before each stream-module start, boot/session is incremented and saved. The host switches clock mappings by session.

BMI088 timestamps are marked valid when they use actual DRDY timestamps; polling fallback sets a quality flag. The current ICM driver polls the FIFO without reconstructing exact per-sample hardware timing, so ICM remains a diagnostic source. Dual-IMU fusion requires ICM timing reconstruction and calibration of both IMUs' axes, biases, lever arms, and noise.

The host requests synchronization at 10 Hz and uses four software timestamps to fit an MCU-to-host-monotonic-clock mapping, then converts to the ROS clock. Expiration or a ROS clock jump invalidates the mapping. Absolute accuracy of this software synchronization still requires independent measurement. See the [Protocol](PROTOCOL.md) for timestamp locations.

## CAN Transport

The H7 FDCAN driver handles filtering, transmission, reception, timeout cleanup, and shutdown. The module initializes the driver clock and provides controller-register queries.

Each 48-byte sample uses 8 standard DLC 8 frames. Every fragment carries a sequence number; the packet CRC covers baseID, sequence number, and payload. Additional traffic consists of 1 Hz status and 10 Hz synchronization. There are 32 transmit slots with a 3 ms frame deadline and expired-frame cleanup. See [PROTOCOL.md](PROTOCOL.md) for standard ID allocation, separate from motor IDs 1–4 and parameter ID 0x7FF.

Host reassembly allows at most 32 incomplete packets with a 20 ms timeout, checking CRC, session, sequence number, increasing timestamps, and finite values. The shared-bus motor receiver filters by the ENCOS ID range before forwarding frames to the motor parser.

## Host and ROS

`host/can_imu/` contains the protocol library, receiver/statistics tools, USB queries, application flashing, and calibration tools. Reception uses only the Python standard library; USB uses pyserial. USB tools query and configure the device through the NSH console.

`ros2/fc_clamp_can_imu` installs the same protocol implementation and publishes `/ee_imu/raw`, `/ee_imu/data`, and diagnostics. By default, the primary data topic requires clock synchronization, complete samples, no clipping, actual sampling timestamps, and valid calibration. Temperature stability can also be required. The first orientation covariance entry is −1 and the six-axis covariance is 0, indicating unavailable orientation and unknown covariance, respectively.

USB maintenance processes share a hardware mutex; synchronization processes use a separate single-instance lock. The parent project's ROS workspace references this repository's package through a symbolic link.

## Calibration and Temperature Control

Each IMU independently stores three-axis accelerometer and gyroscope biases, scales, and temperature slopes relative to 48°C. Defaults are zero bias/slope, unit scale, and invalid calibration flags. The stationary-gyroscope and six-face accelerometer tools generate parameter commands for review. Set calibration-valid flags after calibration and residual validation.

Each optional heater uses PI control, a maximum 60% duty cycle, 200 ms stale-temperature detection, and latched faults for 60°C overtemperature and a five-minute warm-up timeout. Stopping the module disables both heaters. Heaters are disabled by default; complete physical thermal tests and fault-injection validation before enabling them.

## Validation and Integration

Offline protocol/integration tests, USB flashing, sensor readings, stationary shared-bus observation, and two bounded motor comparison runs are complete. See [VALIDATION.md](VALIDATION.md) for continuous-stream and live ROS-stream results, and [SOURCE_LAYOUT.md](SOURCE_LAYOUT.md) for build results after source trimming. Six-face calibration, extrinsics, dynamic delay, and EKF integration remain future hardware work.
