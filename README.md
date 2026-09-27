# DM-FC01 Standalone CAN IMU

Target hardware: STM32H743, BMI088, and ICM45686. The firmware streams six-axis measurements and device timestamps over classic CAN, with USB configuration, CAN clock synchronization, and ROS 2 receiver tools.

BMI088 provides the primary 200 Hz stream; ICM45686 is used for diagnostics. Each sample contains three-axis acceleration, three-axis angular velocity, and an MCU timestamp. The host maps device time to the ROS clock through CAN synchronization.

In hardware tests on a shared bus, four motors controlled at 150 Hz plus a 200 Hz IMU stream used approximately 36.0%–43.7% of bus capacity. A 30-minute recording received 359995 samples with no missing samples. These hardware results apply to the flashed version. The current trimmed source has passed a full build and offline checks and awaits flashing and hardware validation.

| Document | Contents |
|---|---|
| [Protocol](docs/can-imu/PROTOCOL.md) | CAN IDs, fragmentation, fields, and clock synchronization |
| [Implementation](docs/can-imu/IMPLEMENTATION_PLAN.md) | Board wiring, sampling, filtering, and timestamps |
| [Build and Recovery](docs/can-imu/BUILD_AND_RECOVERY.md) | USB flashing, recovery, and CAN diagnostics |
| [Source Layout](docs/can-imu/SOURCE_LAYOUT.md) | Directories, dependencies, and source-trimming validation |
| [Temperature Control](docs/can-imu/THERMAL_CONTROL.md) | Vendor heater design, control protections, and thermal acceptance |
| [Validation Record](docs/can-imu/VALIDATION.md) | Test conditions, measured results, and pending validation |

## Data and Timestamps

The drivers convert raw FIFO values to m/s² and rad/s, then apply bias correction, temperature compensation, per-axis scaling, two low-pass stages, and averaging over a 5 ms window. Each low-pass stage defaults to 60 Hz, giving a combined −3 dB frequency of approximately 38.6 Hz.

The timestamp marks the end of the sampling window. EKF time alignment must account for approximately 5.3 ms of low-frequency filter group delay and 2.5 ms of window-center delay. The primary ROS topic uses synchronized device time.

Output uses the vendor driver's fixed rotation `-R 4`, preserving the gravity response and actual mounting tilt. Verify the IMU-to-motion-capture extrinsics one axis at a time before integration. BMI088 uses DRDY sampling timestamps; polling fallback sets a time-quality flag. Per-sample timing for ICM45686 and dual-IMU fusion still require implementation and validation.

## Build and Flash

Build tools: CMake, Ninja, GNU Arm Embedded GCC (tested with 13.2.1), newlib, and genromfs. The Python environment requires kconfiglib, pyelftools, toml, empy 3.3.4, and pyros-genmsg. The host receiver uses the Python standard library; USB tools also require pyserial.

Clone the standalone repository and initialize the dependencies used by this target:

```bash
git clone --depth 1 git@github.com:TongZhe2016/dm-fc01-can-imu.git
cd dm-fc01-can-imu
git submodule update --init --recursive -- \
  platforms/nuttx/NuttX/apps platforms/nuttx/NuttX/nuttx \
  src/lib/events/libevents src/lib/heatshrink/heatshrink
```

The following command uses the Python path on the validation host. Replace `PYTHON_EXECUTABLE` on other machines:

```bash
make damiao_dm-fc01_imu -j4 \
  PYTHON_EXECUTABLE=/home/airman/FC-CLAMP_Real_Drone/.omx/venvs/can-imu-build/bin/python
```

The output is `build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4`, with board ID 7140. Run only one build process per build directory.

From the repository root, flash the application after confirming that the target serial port belongs to the DM-FC01:

```bash
python3 host/can_imu/flash.py \
  build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4
```

When the USB console is working, the flashing tool can reboot the application into the bootloader in software, without pressing the Boot button. Flashing updates only the application. See [Build and Recovery](docs/can-imu/BUILD_AND_RECOVERY.md) for USB enumeration failures, device resets, and recovery images.

## USB Configuration

```bash
python3 host/can_imu/usb_query.py --plain 'can_imu status' 'bmi088 -A status'
```

The module reads parameters at startup. After changing them, run `param save`, then restart the module with `can_imu stop` and `can_imu start`. Each start increments the stream session number.

During initial configuration, verify the CAN1 bitrate of 1 Mbps and the IDs on the shared bus, then set and save `CI_AUTOSTART=1`. The firmware default is 0; autostart is enabled on the validation board.

| Parameter | Meaning |
|---|---|
| `CI_AUTOSTART` | Start automatically at boot; default 0. Enable after checking bus IDs |
| `CI_SOURCE` | 0: BMI088; 1: ICM45686 diagnostics; other values prevent startup |
| `CI_TX_EN` | Enable the 200 Hz primary stream; 1 Hz status and synchronization replies remain active when disabled |
| `CI_HEAT_EN` | Bitmask: 1 BMI088, 2 ICM45686, 3 both; default 0 |
| `CI_HEAT_T` | Heater target; default 48°C, allowed range 30–50°C |
| `CI0_HEAT_P/I`, `CI1_HEAT_P/I` | Per-IMU PI gains; source defaults 0.10/0.01. See the thermal tuning record for tested board settings |
| `CI_LPF_HZ` | Pole frequency of each low-pass stage; default 60 Hz, allowed range 1–90 Hz |
| `CI_EPOCH` | Increment after changing the primary source, calibration, or filtering to notify the receiver of a configuration change |
| `CI_BOOT` | Stream session counter, incremented and persisted before each module start; do not manually reuse old values |
| `CI0_AXB/AXS/AXT`, etc. | IMU0, acceleration X: bias/scale/temperature slope; independent values for 0/1, A/G, and X/Y/Z |
| `CI0_ACAL/GCAL`, etc. | Corresponding calibration-valid flags; default 0 |

Heaters are disabled by default. The implementation provides independent PI controllers, a maximum 60% duty cycle, a 200 ms temperature-update timeout, and latched faults for NaN, 60°C overtemperature, and failure to warm up within five minutes. A fault disables the affected heater; stopping the module disables both GPIO outputs. Complete physical thermal tests and fault-injection validation before enabling the heaters. See [Temperature Control](docs/can-imu/THERMAL_CONTROL.md) for the previous firmware design, temperature-read freshness checks, and acceptance procedure.

The calibration tool generates USB parameter commands for review before writing them to the device:

```bash
python3 host/can_imu/calibrate.py --gyro samples.jsonl --source 0 --output gyro-cal.json
python3 host/can_imu/calibrate.py --faces faces.json --source 0 --output accel-cal.json
```

Record at least five seconds of stationary, continuous data at the actual operating temperature. Before recording, set the corresponding biases and temperature slopes to 0 and scales to 1. The six-face JSON maps `x+`, `x-`, `y+`, `y-`, `z+`, and `z-` to six logs. The tool checks motion, labeled axes, and value ranges. After writing the parameters, check residuals using an independent six-face test. Obtain temperature slopes through dedicated thermal calibration.

## Host Receiver and ROS 2

```bash
python3 host/can_imu/receive.py --interface can0 --duration 30 \
  --sync --observe-bus --output /tmp/imu-record
```

The receiver uses the existing `can0` configuration. `--sync` enables CAN clock synchronization; only one synchronization process is allowed per interface. `--observe-bus` subscribes to the entire bus to estimate load. Without this option, only IMU frames are received.

Logs contain samples, status, and synchronization events. The summary reports frequency, missing samples, arrival intervals, sample age, synchronization residuals, axis statistics, and socket drops. Bus load is estimated from frame lengths and bit-stuffing bounds; timing statistics include host scheduling delays. See the [Protocol](docs/can-imu/PROTOCOL.md) for timestamp locations and synchronization accuracy.

ROS package: `ros2/fc_clamp_can_imu`. It installs the same Python parsing code from this repository's `host/can_imu` directory.

```bash
. /opt/ros/jazzy/setup.bash
colcon --log-base .omx/log/can-imu-ros build --base-paths ros2/fc_clamp_can_imu \
  --build-base .omx/build/can-imu-ros --install-base .omx/install/can-imu-ros \
  --packages-select fc_clamp_can_imu
. .omx/install/can-imu-ros/setup.bash
ros2 launch fc_clamp_can_imu imu.launch.py
```

- `/ee_imu/raw`: diagnostic data; uses reception time until synchronization is available. Use diagnostics to determine its timestamp semantics.
- `/ee_imu/data`: synchronized, valid samples, with calibration required by default. Rejects polling timestamps and stale samples. `require_warm` can also require temperature stability.
- `/ee_imu/diagnostics`: JSON status, session, configuration version, and counters.

In `sensor_msgs/Imu`, `orientation_covariance[0]=-1` indicates unavailable orientation; zero six-axis covariance indicates unknown covariance. Connect `/ee_imu/data` to the EKF after validating coordinates, noise, and timing delays.

## Offline Checks

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu \
  host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
```

See [PROTOCOL.md](docs/can-imu/PROTOCOL.md) for the protocol layout and [VALIDATION.md](docs/can-imu/VALIDATION.md) for hardware results and outstanding work.

## Maintenance

USB tools use the NSH console by default and accept the `--plain` option. The flashing tool checks the board ID and acquires a hardware mutex: `.omx/state/hardware-can.lock` in the parent project, or `$XDG_STATE_HOME/dm-fc01-can-imu/hardware.lock` in a standalone checkout, with `~/.local/state` as the default state directory. Set `CAN_IMU_HARDWARE_LOCK` to specify a shared lock path.

The NuttX kernel, NuttX application support, libevents, and heatshrink are maintained as submodules pinned to specific commits. Git history retains the hardware-validated versions and the source-trimming baseline. See [Source Layout](docs/can-imu/SOURCE_LAYOUT.md) for directory and version details.
