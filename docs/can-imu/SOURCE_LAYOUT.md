# CAN IMU 源码与构建

支持目标：`damiao_dm-fc01_imu`，STM32H743 + BMI088 + ICM45686。入口为 `make`；可以通过 `BUILD_DIR`、`JOBS` 和 `PYTHON_EXECUTABLE` 指定构建目录、并行度及 Python 环境。

## 目录

| 路径 | 用途 |
|---|---|
| `src/modules/can_imu` | 六轴处理、窗口时间戳、CAN 分片、设备对时、参数及可选温控 |
| `src/drivers/imu` | BMI088 与 ICM45686 SPI/FIFO 驱动 |
| `src/drivers/uavcan` | STM32H7 CAN 硬件后端及所需传输接口、时钟类型 |
| `src/drivers/cdcacm_autostart`、`src/systemcmds` | USB 控制台、刷写入口和运行诊断 |
| `src/lib`、`src/include` | 参数持久化、事件、容器、传感器封装和数学基础 |
| `boards/damiao/dm-fc01` | 本板 GPIO、SPI、USB、启动和链接配置 |
| `platforms/common`、`platforms/nuttx` | 任务调度、uORB、计时及 STM32H7 平台适配 |
| `msg` | 本固件使用的 10 个消息定义 |
| `ROMFS/can_imu` | 应用启动脚本 |
| `Tools`、`cmake` | 消息/参数生成、固件打包及 USB 上传工具 |
| `host/can_imu`、`ros2` | CAN 接收、对时、标定辅助、USB 维护、ROS 2 适配与测试 |
| `docs/can-imu` | 协议、构建、实机验收和维护记录 |

## 基础依赖

四个子模块分别为 NuttX 内核、NuttX 应用支持、libevents 和 heatshrink。提交号由 gitlink 固定。它们保留上游源码包，具体组件由本目标配置选择；目录统计应将这些依赖与主仓库分别计算。NuttX 提供 MCU 启动、中断、线程、USB、文件系统及基础驱动，构建通过其配置系统选择所需组件。

CAN 后端取自厂商提供的 libuavcan/STM32H7 驱动，保留许可证和源码版权标记。当前传输接口直接使用微秒时间类型；本地时间类型的运算由 `host/can_imu/tests/transport_time_test.cpp` 验证。

## 裁剪基线

裁剪比较基线为提交 `c59e32f56b`。主仓库原有 13,236 个文件、384,780,845 字节；裁剪后约 734 个文件、4.1 MB。统计排除 Git 历史、构建产物和子模块内部文件。

源码删除包括飞行控制/导航/估计算法、其他板型、仿真和训练资源，以及不属于本构建的驱动、消息和开发工具。构建入口、参数生成和固件内元数据也随目标收敛。现有协议、传感器旋转、滤波与窗口时间戳语义见 [PROTOCOL.md](PROTOCOL.md) 和 [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)。

Git 历史保留基线和硬件验收版本。首次获取可使用 `git clone --depth 1`；浅克隆仍需按 README 初始化四个依赖。

## 验证

使用全新的 `build/imu-clean-final` 目录执行：

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

全新构建目录和独立浅克隆均完整编译通过，生成 board ID 7140 的 `.px4` 应用包。浅克隆的依赖从本机同提交镜像初始化。

| 构建版本 | 应用镜像大小 |
|---|---:|
| 本地空目录构建 | 256,364 字节 |
| 提交 `324c2b2ebd` 的浅克隆构建 | 256,380 字节 |
| 裁剪前已刷入版本 | 298,436 字节 |

两个裁剪版镜像均约 256 KB，包含各自的版本元数据。协议 7 项测试、C++ ASan/UBSan 核心检查和 CAN 时间类型检查已通过。

源码裁剪产物待刷入验证。实机结果对应 [VALIDATION.md](VALIDATION.md) 中记录的已刷入版本。
