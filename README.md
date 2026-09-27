# DM-FC01 独立 CAN IMU

目标硬件：STM32H743、BMI088、ICM45686。应用基于厂商 PX4/NuttX 板级支持，输出六轴测量，不启动飞行控制或姿态估计模块。

已在 DM-FC01 上完成 USB 应用刷写、200 Hz CAN 输出及四电机共线对照。1 Mbps 总线上，150 Hz 四轴控制加 200 Hz IMU 的估计占用为 **36.0%–43.7%**；本次对照无 IMU 缺样、电机过期反馈或 CAN 错误。测试范围和持续时间见 [验收记录](docs/can-imu/VALIDATION.md)。

CAN 使用标准帧：样本 `0x680–687`、状态 `0x6A0–6A7`、对时回复 `0x6B0–6B3`、对时请求 `0x6C0`。ENCOS 电机保持 ID 1–4，配置帧为 0x7FF。首次配置默认不自动输出；核对共享总线 ID 后设置 `CI_AUTOSTART=1` 并保存。

## 数据链

- 两颗传感器分别消费原始 FIFO；乘消息中的 scale 换算为 m/s² 和 rad/s。
- 保留厂商驱动固定旋转 `-R 4`。没有水平校正、重力扣除或世界坐标旋转。垂直安装不改变这条链。三轴方向仍需要实物逐轴验证，之后核对 IMU→动捕外参。
- 每颗传感器的处理顺序：减偏置和温度补偿（参考 48°C）→逐轴比例→两级一阶低通→同一设备时间网格上的 5 ms 窗口平均。
- 每级低通极点默认 60 Hz；组合的 −3 dB 频率约 38.6 Hz，低频群延迟约 5.3 ms。窗口平均另有约 2.5 ms 的中心延迟。时间字段是窗口末端，不是补偿过群延迟的等效瞬时采样时间。
- BMI088 默认主流 200 Hz；DRDY 回退到轮询时明确置时间质量位。ICM45686 的当前轮询时间戳使其仅适合诊断；未实现已验证的双 IMU 融合。
- 任何缺口超过三倍 nominal dt 都不插值补齐；覆盖不完整的窗口丢弃并计数。FIFO 发布队列深度为 16，消费端记录丢更新。

## 构建和刷写

本仓库保留厂商 PX4/NuttX 板级源码和许可证，基线 `b3d0fad488ac158d7af8cafb5de2b4ef8bc162b3`。

当前主机使用 ARM GCC 13.2.1、Ninja、已有 NuttX 子模块，以及本机 `.omx/venvs/can-imu-build` 中的构建期 `pyros-genmsg`。主机接收器使用 Python 标准库；USB 工具使用已有 pyserial。

```bash
cd dm-fc01-can-imu
make damiao_dm-fc01_imu -j4 \
  PYTHON_EXECUTABLE=/home/airman/FC-CLAMP_Real_Drone/.omx/venvs/can-imu-build/bin/python
```

产物：`build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4`，board ID 必须为 7140。不要并发运行同一 build 目录的构建。固件包含独立 ROMFS `ROMFS/can_imu`，USB 先启动，随后加载参数和传感器，`CI_AUTOSTART=1` 时启动 CAN。

在仓库根目录，确认目标串口为 DM-FC01 后刷写应用：

```bash
python3 host/can_imu/flash.py \
  build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4
```

已实测正常 USB 控制台可软件进入 bootloader，无需 Boot 键；应用卡死且 USB 消失时，电脑无法再发复位指令。先准备 uploader，再现场 RESET/完整断电上电捕获 bootloader。仅重置主机 USB 端口不保证复位独立供电的 MCU。

本任务恢复镜像保存在本机 `.omx/artifacts/dm-fc01-imu/usb-rescue.px4`，只启动 USB。原厂应用恢复参考为 `real_drone/docs/dm-fc01/固件/PX4/damiao_dm-fc01_V1.16.px4`；它与最初板上应用不是同一 git hash。原参数备份为本机 `factory-parameters.json` 和 `preflash.json`。刷写不更新 bootloader。

恢复细节与本次 Jetson USB 故障处理见 [BUILD_AND_RECOVERY.md](docs/can-imu/BUILD_AND_RECOVERY.md)。

## USB 配置

```bash
python3 host/can_imu/usb_query.py --plain 'can_imu status' 'bmi088 -A status'
```

参数在模块启动时读取；更改后 `param save`，再执行 `can_imu stop` 和 `can_imu start`。每次启动递增流会话。已验证 USB 下停止/重启；连续 CAN 恢复仍在验收。

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

加热器默认关闭。实现了独立 PI、最大 60% 占空比、温度更新超时 200 ms、NaN/60°C 过温和五分钟未升温故障锁存。故障后关闭相应加热器；停止模块关闭两路 GPIO。保护实现和温控性能尚未通过实物热测试。

标定工具输出可审核的 USB 参数命令，不自动写板：

```bash
python3 host/can_imu/calibrate.py --gyro samples.jsonl --source 0 --output gyro-cal.json
python3 host/can_imu/calibrate.py --faces faces.json --source 0 --output accel-cal.json
```

录制前将对应偏置和温度斜率设为 0、比例设为 1；五秒以上静置、完整连续记录，使用实际工作温度。六面 JSON 将 `x+、x-、y+、y-、z+、z-` 映射到六份日志。工具检查运动、标签轴和合理范围，但不能代替独立六面残差验收。当前安装只允许 ±5° 电机运动，不能自动完成六面标定。温度斜率需专门热标定，本工具不拟合它。

## 主机接收与 ROS 2

```bash
python3 host/can_imu/receive.py --interface can0 --duration 30 \
  --sync --observe-bus --output /tmp/imu-record
```

保持现有 can0 配置；接收器不修改波特率、不关闭共享总线。对时发送仅使用协议白名单 ID，由独立实例锁限制单写入者。电机工具继续持有原硬件锁。`--observe-bus` 用于负载统计；不加时只订阅 IMU ID。负载给出标准/扩展经典帧位填充的上下界，不是示波器实测利用率。

输出包括完整样本、状态、对时事件和 summary；有效频率、缺样、到达间隔、估计样本年龄、同步拟合残差、噪声和 socket 丢包分开报告。Linux 用户态接收时间包含主机调度延迟。设备 t2 是驱动出队时间，t3 是回复排队前时间；这不是硬件收发打点，拟合残差不能证明绝对同步精度。

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

Imu 消息不提供姿态，`orientation_covariance[0]=-1`。六轴协方差保持未知，不冒充实测噪声。没有自动改动现有 EE EKF 的 MAVROS 启动和外参；坐标、噪声和时间延迟验收后再切换输入。

## 离线检查

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu \
  host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
```

协议布局见 [PROTOCOL.md](docs/can-imu/PROTOCOL.md)，硬件结果及未完成项见 [VALIDATION.md](docs/can-imu/VALIDATION.md)。

## 独立仓库依赖与维护

仅初始化 IMU 构建所需子模块：

```bash
git submodule update --init -- platforms/nuttx/NuttX/apps platforms/nuttx/NuttX/nuttx \
  src/drivers/uavcan/libdronecan/dsdl \
  src/drivers/uavcan/libdronecan/libuavcan/dsdl_compiler/pydronecan \
  src/lib/events/libevents src/lib/heatshrink/heatshrink src/modules/mavlink/mavlink
```

构建工具：CMake、Ninja、GNU Arm Embedded GCC（已测 13.2.1）、newlib、genromfs，以及 Python 的 kconfiglib、pyelftools、toml、empy 3.3.4、pyros-genmsg。使用 `PYTHON_EXECUTABLE` 指定已有环境；README 上面的绝对路径是本次测试环境，可替换为自己的 Python。

USB 独立固件使用 `usb_query.py --plain`，仅依赖 pyserial。`backup_parameters.py` 和未加 `--plain` 的查询仅用于原厂 MAVLink 固件，需环境中已有 pymavlink；独立 IMU 固件不提供 MAVLink。工具不再引用旧参考仓库。

推荐刷写入口 `python3 host/can_imu/flash.py build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4` 会检查 board ID 并获取互斥锁。在主项目内使用主项目 `.omx/state/hardware-can.lock`；独立检出使用 `$XDG_STATE_HOME/dm-fc01-can-imu/hardware.lock`（默认 `~/.local/state`）。可用 `CAN_IMU_HARDWARE_LOCK` 显式指定共享锁。

保留的上游介绍见 [README.px4.md](README.px4.md)，实现范围见 [IMPLEMENTATION_PLAN.md](docs/can-imu/IMPLEMENTATION_PLAN.md)。本仓库仍包含上游其他板型源码，实际交付构建目标为 `damiao_dm-fc01_imu`。
