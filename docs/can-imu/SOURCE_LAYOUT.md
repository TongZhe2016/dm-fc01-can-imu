# CAN IMU Source Layout and Build

Supported target: `damiao_dm-fc01_imu`, STM32H743 + BMI088 + ICM45686. Use `make` as the build entry point. Set `BUILD_DIR`, `JOBS`, and `PYTHON_EXECUTABLE` to select the build directory, parallelism, and Python environment.

## Directories

| Path | Purpose |
|---|---|
| `src/modules/can_imu` | Six-axis processing, window timestamps, CAN fragmentation, device clock synchronization, parameters, and optional temperature control |
| `src/drivers/imu` | BMI088 and ICM45686 SPI/FIFO drivers |
| `src/drivers/uavcan` | STM32H7 CAN hardware backend, required transport interfaces, and clock types |
| `src/drivers/cdcacm_autostart`, `src/systemcmds` | USB console, flashing entry point, and runtime diagnostics |
| `src/lib`, `src/include` | Parameter persistence, events, containers, sensor wrappers, and math primitives |
| `boards/damiao/dm-fc01` | Board GPIO, SPI, USB, startup, and linker configuration |
| `platforms/common`, `platforms/nuttx` | Task scheduling, uORB, timing, and STM32H7 platform support |
| `msg` | The 10 message definitions used by this firmware |
| `ROMFS/can_imu` | Application startup scripts |
| `Tools`, `cmake` | Message/parameter generation, firmware packaging, and USB upload tools |
| `host/can_imu`, `ros2` | CAN reception, clock synchronization, calibration helpers, USB maintenance, ROS 2 integration, and tests |
| `docs/can-imu` | Protocol, build, hardware validation, and maintenance records |

## Core Dependencies

The four submodules are the NuttX kernel, NuttX application support, libevents, and heatshrink. Their commits are pinned by gitlinks. They retain their upstream source distributions, with components selected by this target's configuration. Count these dependencies separately from the main repository when reporting directory statistics. NuttX provides MCU startup, interrupts, threads, USB, filesystems, and basic drivers; its configuration system selects the required build components.

The CAN backend comes from the vendor-provided libuavcan/STM32H7 driver, with license and source copyright notices retained. The current transport interface uses microsecond time types directly. Operations on the local time types are verified by `host/can_imu/tests/transport_time_test.cpp`.

## Source-Trimming Baseline

Commit `c59e32f56b` is the comparison baseline. The main repository originally contained 13,236 files totaling 384,780,845 bytes; after trimming, it contains approximately 734 files totaling 4.1 MB. These counts exclude Git history, build outputs, and files inside submodules.

Removed sources include flight-control, navigation, and estimation algorithms; other boards; simulation and training resources; and drivers, messages, and development tools outside this build. Build entry points, parameter generation, and firmware metadata were also narrowed to this target. See [PROTOCOL.md](PROTOCOL.md) and [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) for the existing protocol, sensor rotations, filtering, and window-timestamp semantics.

Git history retains the baseline and hardware-validated versions. Use `git clone --depth 1` for an initial shallow checkout if desired; initialize the four dependencies as described in the README even for a shallow clone.

## Verification

Run the following with a fresh `build/imu-clean-final` directory:

```bash
make BUILD_DIR=build/imu-clean-final JOBS=4 PYTHON_EXECUTABLE=/path/to/build-env/bin/python
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
g++ -std=c++14 -Wall -Wextra -Werror -DUAVCAN_CPP_VERSION=2003 \
  -DUAVCAN_TOSTRING=0 -DUAVCAN_NO_ASSERTIONS \
  -Isrc/drivers/uavcan/libdronecan/libuavcan/include \
  host/can_imu/tests/transport_time_test.cpp -o /tmp/can-imu-transport-time-test
/tmp/can-imu-transport-time-test
python3 -m compileall -q host/can_imu ros2/fc_clamp_can_imu
bash -n ROMFS/can_imu/init.d/rcS
```

Full builds passed both in a fresh build directory and in a standalone shallow clone, producing `.px4` application packages with board ID 7140. The shallow clone's dependencies were initialized from local mirrors at the same commits.

| Build version | Application image size |
|---|---:|
| Local build in an empty directory | 256,364 bytes |
| Shallow-clone build of commit `324c2b2ebd` | 256,380 bytes |
| Flashed version before source trimming | 298,436 bytes |

Both trimmed images are approximately 256 KB and contain their respective version metadata. All 7 protocol tests, the C++ ASan/UBSan core checks, and the CAN time-type checks passed.

The trimmed-source build awaits flashing and hardware validation. Hardware results apply to the flashed version recorded in [VALIDATION.md](VALIDATION.md).
