# DM-FC01 独立 CAN IMU

目标硬件：STM32H743、BMI088、ICM45686。固件通过经典 CAN 输出六轴测量和设备时间戳，提供 USB 配置、CAN 对时和 ROS 2 接收工具。

BMI088 提供 200 Hz 主流，ICM45686 用于诊断。每个样本包含三轴加速度、三轴角速度和 MCU 时间戳；主机通过 CAN 对时将设备时间映射到 ROS 时钟。

实机共线测试中，四电机 150 Hz 控制加 200 Hz IMU 的总线占用约为 36.0%–43.7%，30 分钟记录收到 359995 个样本，零缺样。实机结果对应已刷入版本；当前源码裁剪版已通过完整构建和离线检查，待刷入验证。

| 文档 | 内容 |
|---|---|
| [协议](docs/can-imu/PROTOCOL.md) | CAN ID、分片、字段和对时规则 |
| [实现](docs/can-imu/IMPLEMENTATION_PLAN.md) | 板级接线、采样、滤波和时间戳 |
| [构建与恢复](docs/can-imu/BUILD_AND_RECOVERY.md) | USB 刷写、恢复和 CAN 诊断 |
| [源码组成](docs/can-imu/SOURCE_LAYOUT.md) | 目录、依赖和裁剪验证 |
| [验收记录](docs/can-imu/VALIDATION.md) | 测试条件、实测结果和待验证项 |

## 数据与时间戳

驱动将 FIFO 原始值换算为 m/s² 和 rad/s，再依次应用偏置、温度补偿、逐轴比例、两级低通和 5 ms 窗口平均。每级低通默认 60 Hz，组合 −3 dB 频率约 38.6 Hz。

时间戳表示采样窗口的末端。低通约 5.3 ms 的低频群延迟和窗口约 2.5 ms 的中心延迟需要在 EKF 时间对齐时考虑。正式 ROS 话题使用同步后的设备时间。

输出采用厂商驱动固定旋转 `-R 4`，保留重力响应和实际安装倾角。接入前需逐轴核对 IMU→动捕外参。BMI088 使用 DRDY 采样时刻；轮询回退会设置时间质量位。ICM45686 的逐样本计时和双 IMU 融合仍待实现、验证。

## 构建和刷写

构建工具为 CMake、Ninja、GNU Arm Embedded GCC（已测 13.2.1）、newlib 和 genromfs。Python 环境需要 kconfiglib、pyelftools、toml、empy 3.3.4 和 pyros-genmsg。主机接收器使用 Python 标准库，USB 工具另需 pyserial。

独立检出并初始化本目标使用的依赖：

```bash
git clone --depth 1 git@github.com:TongZhe2016/dm-fc01-can-imu.git
cd dm-fc01-can-imu
git submodule update --init --recursive -- \
  platforms/nuttx/NuttX/apps platforms/nuttx/NuttX/nuttx \
  src/lib/events/libevents src/lib/heatshrink/heatshrink
```

下面使用验收主机的 Python 路径；在其他电脑上替换 `PYTHON_EXECUTABLE`：

```bash
make damiao_dm-fc01_imu -j4 \
  PYTHON_EXECUTABLE=/home/airman/FC-CLAMP_Real_Drone/.omx/venvs/can-imu-build/bin/python
```

产物为 `build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4`，board ID 为 7140。同一构建目录只运行一个构建进程。

在仓库根目录，确认目标串口为 DM-FC01 后刷写应用：

```bash
python3 host/can_imu/flash.py \
  build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4
```

USB 控制台正常时，刷写工具可让应用软件重启进入 bootloader，无需按 Boot 键。刷写只更新应用。USB 枚举失败、设备复位及恢复镜像的处理见[构建与恢复](docs/can-imu/BUILD_AND_RECOVERY.md)。

## USB 配置

```bash
python3 host/can_imu/usb_query.py --plain 'can_imu status' 'bmi088 -A status'
```

模块启动时读取参数。修改后执行 `param save`，再用 `can_imu stop`、`can_imu start` 重启模块。每次启动递增流会话编号。

首次配置时核对 CAN1 的 1 Mbps 波特率和共享总线 ID，再设置并保存 `CI_AUTOSTART=1`。固件默认值为 0；验收板已启用上电自启。

| 参数 | 含义 |
|---|---|
| `CI_AUTOSTART` | 开机自动启动，默认 0；核对总线 ID 后开启 |
| `CI_SOURCE` | 0 BMI088；1 ICM45686 诊断；其他值拒绝启动 |
| `CI_TX_EN` | 200 Hz 主流开关；关闭后仍有 1 Hz 状态和对时回复 |
| `CI_HEAT_EN` | 位掩码：1 BMI088、2 ICM45686、3 两者；默认 0 |
| `CI_HEAT_T` | 加热目标，默认 48°C；允许 30–50°C |
| `CI_LPF_HZ` | 每级低通极点频率，默认 60 Hz，允许 1–90 Hz |
| `CI_EPOCH` | 修改主源/标定/滤波后增加，通知接收端配置变化 |
| `CI_BOOT` | 每次模块启动前持久化递增的流会话；勿手工复用旧值 |
| `CI0_AXB/AXS/AXT` 等 | IMU0、加速度X：偏置/比例/温度斜率；0/1、A/G、X/Y/Z 均独立 |
| `CI0_ACAL/GCAL` 等 | 对应标定有效标记，默认 0 |

加热器默认关闭。实现了独立 PI、最大 60% 占空比、温度更新超时 200 ms、NaN/60°C 过温和五分钟未升温故障锁存。故障时关闭对应加热器，模块停止时关闭两路 GPIO。启用前需完成实物热测试和故障注入验证。

标定工具生成 USB 参数命令，审核后再写入设备：

```bash
python3 host/can_imu/calibrate.py --gyro samples.jsonl --source 0 --output gyro-cal.json
python3 host/can_imu/calibrate.py --faces faces.json --source 0 --output accel-cal.json
```

在实际工作温度下录制至少五秒静止、连续数据。录制前将对应偏置和温度斜率设为 0、比例设为 1。六面 JSON 将 `x+、x-、y+、y-、z+、z-` 映射到六份日志。工具检查运动、标签轴和数值范围；写入参数后还需检查独立六面残差。温度斜率通过专门热标定获取。

## 主机接收与 ROS 2

```bash
python3 host/can_imu/receive.py --interface can0 --duration 30 \
  --sync --observe-bus --output /tmp/imu-record
```

接收器使用已有的 `can0` 配置。`--sync` 开启 CAN 对时，同一接口只允许一个对时进程；`--observe-bus` 订阅全总线，用于估计负载。省略该选项时只接收 IMU 帧。

日志记录样本、状态和对时事件，summary 汇总频率、缺样、到达间隔、样本年龄、同步残差、轴统计及 socket 丢包。总线负载按报文长度和位填充上下界估计；时间统计包含主机调度延迟。对时打点和精度说明见[协议](docs/can-imu/PROTOCOL.md)。

ROS 包：`ros2/fc_clamp_can_imu`。它安装本仓库 `host/can_imu` 的同一套 Python 解析代码。

```bash
. /opt/ros/jazzy/setup.bash
colcon --log-base .omx/log/can-imu-ros build --base-paths ros2/fc_clamp_can_imu \
  --build-base .omx/build/can-imu-ros --install-base .omx/install/can-imu-ros \
  --packages-select fc_clamp_can_imu
. .omx/install/can-imu-ros/setup.bash
ros2 launch fc_clamp_can_imu imu.launch.py
```

- `/ee_imu/raw`：诊断数据；尚未对时则使用接收时刻。配合 diagnostics 判断其时间语义。
- `/ee_imu/data`：时间已同步、样本有效、默认要求已标定；拒绝轮询时间戳及过期样本。`require_warm` 可要求温稳。
- `/ee_imu/diagnostics`：JSON 状态、会话、配置版本、计数器。

`sensor_msgs/Imu` 中，`orientation_covariance[0]=-1` 表示姿态不可用；六轴协方差为 0，表示未知。完成坐标、噪声和时间延迟验收后，再将 `/ee_imu/data` 接入 EKF。

## 离线检查

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu \
  host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
```

协议布局见 [PROTOCOL.md](docs/can-imu/PROTOCOL.md)，硬件结果及未完成项见 [VALIDATION.md](docs/can-imu/VALIDATION.md)。

## 维护

USB 工具默认使用 NSH 控制台，兼容 `--plain` 参数。刷写工具检查 board ID 并获取硬件互斥锁：在主项目中使用 `.omx/state/hardware-can.lock`，独立检出使用 `$XDG_STATE_HOME/dm-fc01-can-imu/hardware.lock`，默认位于 `~/.local/state`。`CAN_IMU_HARDWARE_LOCK` 可指定共享锁路径。

NuttX 内核、NuttX 应用支持、libevents 和 heatshrink 以固定提交的子模块维护。Git 历史保留硬件验收版本和裁剪基线；目录与版本说明见[源码组成](docs/can-imu/SOURCE_LAYOUT.md)。
