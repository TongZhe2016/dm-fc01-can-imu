# 早期调试记录（历史状态）

本页归档 2026-09-27 从首次刷写到四轴参考标定的过程，保留当时的故障、判断、命令和后续计划。各节状态均对应记录时点。最终测试结果见 [VALIDATION.md](VALIDATION.md)，现行操作入口见 [README](../../README.md)。

历史命令使用迁移前的目录，仅供追溯。

## 首轮实现与 USB 失联

日期：2026-09-27。该阶段已完成软件实现与离线检查，USB 失联阻塞了实机调试，固件和电机共线验收待恢复后进行。

### 阶段结果

| 项目 | 证据与结果 |
|---|---|
| 原板识别 | DM-FC01，STM32H743 rev V，board ID7140；原应用hash `a4ad2fc346a543271cfc2c17a4baa21f14eb5f79`，PX4 1.16.1 |
| 参数备份 | 1019个已使用参数；浮点来自MAVLink binary32，整数以同时保存的NSH文本核对。MAVLink伪参数 `_HASH_CHECK` 排除；最后的 `WV_YRATE_MAX=90` 由NSH补齐。备份在本机artifact目录 |
| 专用应用构建 | ARM GCC13.2.1、NuttX12.12.0；最终候选board ID7140，image_size297196，最大1835008字节；成功链接和打包 |
| 最新候选SHA256 | `f64931f64aeba30a67b796e1c4feec0815d5f7675f0e78abaf46a64cbd1000c9`；此候选尚未刷写/实机验证 |
| USB软件进入bootloader | 四次开发镜像均完成擦写、编程、CRC校验；未按Boot、未改写bootloader。bootloader `PX4BLv1.16.1g99ad4703c9` |
| USB读取传感器 | 第三次镜像：BMI088 accel FIFO625us/1600Hz、gyro500us/2000Hz；寄存器/传输/溢出/漏DRDY计数均0，初始化reset1；ICM45686之前读取正常 |
| CAN过滤器缺陷定位 | 首次应用重启循环，硬故障PC解析到厂商 `CanIface::configureFilters`。原实现错误地把寄存器偏移OR掩码作为CPU地址；已改用message RAM地址，并修正mask、FIFO路由和数量字段 |
| CAN启动后续 | 第三次应用返回 `CAN filter failed`，没有有效CAN IMU样本；之后调整INIT等待为有界等待并补充停止时中断清理，这些改动仍待实机验证 |
| 当前硬件阻塞 | 第四次应用刷写校验成功后USB枚举失败；主机日志 `device descriptor read/64, error -110`、`device not accepting address, error -71`。只复位对应USB端口未恢复。该次启动脚本未自动启动CAN，不能据此把USB故障归因于CAN过滤器 |
| 协议离线 | 6个Python unittest通过：CRC参考、乱序/重复、丢片/超时/CRC错误、序号回绕/重启/旧会话、丢样/内存上界、时钟漂移/失效 |
| C++处理核心 | g++启用ASan/UBSan通过：不同ODR共同窗口、均值、缺口拒绝、逆序时间戳、裁剪、跨语言CRC向量 |
| ROS 2 | `fc_clamp_can_imu` colcon构建成功；独立domain173运行5秒并以SIGINT结束，无异常输出；没有真实样本，因此未验证ROS数据频率和延迟 |
| 电机前检 | 只读inspect成功，四电机ID1–4、静止、原参数已记录。此前plan和新的局部范围plan均 `live_motion_ready=true` |
| 电机模拟 | 0.5rpm，配置软范围按前检位置±4°，保持原高度/通信/故障保护；完成一次10组动作的采集及释放/参数恢复。调参评分因一个模拟关节未达到90%响应而判基准不可用；这不是实机带宽结论 |
| 测试工具修复 | 失败拟合的inf/NaN记录为JSON null且保留hard_fail，报告支持null；新增单测通过，模拟报告可重新生成 |
| 实机电机运动 | **未执行**。用户已确认现场条件、允许各关节当前位置±5°低速测试；授权持续有效，恢复后需重新只读检查当前位置与占用者 |

### 本机证据

主目录：`/home/airman/FC-CLAMP_Real_Drone/.omx/artifacts/dm-fc01-imu/`。

- `preflash.json`、`factory-parameters.json`：原板、传感器和参数。
- `flash-01.log` 至 `flash-04.log`：四次应用刷写记录。
- `postflash-02.json`：硬故障日志、传感器和USB诊断。
- `postflash-03.json`：1600/2000Hz读取状态及CAN启动错误。
- `can-03/summary.json`：30秒未收到任何IMU样本；不得作为通过验收的记录。
- `candidate.px4`、`candidate-manifest.json`：最新未验证候选。
- `usb-rescue.px4`：只自动启动USB的恢复诊断镜像；尚未成功刷入。
- `recovery-watch.log`：等待实物复位后刷写诊断镜像的有界程序日志。
- `ros-smoke.log`：ROS适配进程smoke，无数据。

电机任务记录：`/home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/session.json`；配置 `bounded.yaml`，模拟记录 `simulate/*/summary.json`。最初实机只读检查在相邻 `imu-can-20260927-inspect` 目录。

### 当时执行的检查命令

在仓库根目录：

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

在 `real_drone/aerial_arm_motor_autotune`：

```bash
PYTHONPATH=tests .venv/bin/python -m unittest test_trace_nonfinite -v
```

ROS构建命令见README，smoke为加载install环境后：

```bash
ROS_DOMAIN_ID=173 timeout --signal=INT 5 ros2 run fc_clamp_can_imu can_imu_node
```

退出124来自有界timeout；节点没有报错。只运行ROS适配进程，不代表有IMU数据或EKF验收。

### 当时的恢复计划

1. 捕获现场RESET/完整断电上电后的bootloader，刷 `usb-rescue.px4`，确认USB稳定。
2. 逐步加载参数、单颗传感器、高速FIFO，再手动启动CAN；保存每一步USB状态与主机枚举日志，定位当前USB故障。
3. 检查扩展帧过滤、收发、CRC、对时、停止/重启与boot session；完整有效200Hz后再刷自动启动候选。
4. 单板连续30分钟，记录有效率/缺样/最长间断/延迟，不用平均频率掩盖断流。逐路热测试和校准另行记录。
5. 实机只读重检、重新核对当前位置±5°范围；两个有界电机批次（每批最多300秒）：关闭IMU主流的基线、打开主流的对照。保持同一运动配置和原参数，采集全总线帧、错误计数差值、电机回复与IMU延迟。
6. 根据上下界负载及电机超时/反馈延迟判定1Mbit/s能否满足当前150Hz控制与200Hz IMU。该阶段尚无带宽验收结论。

未验证项还包括：六面/逐轴方向、外参、陀螺和加速度校准、绝对同步精度、滤波动态延迟、USB拔插独立运行、拥塞/bus-off恢复、温控故障注入，以及EE EKF接入。ICM的精确采样时间恢复与双IMU融合未实施。

## 仓库迁移计划（当时状态）

用户要求在固件工作完成后，将相关实现统一交付到 `git@github.com:TongZhe2016/dm-fc01-can-imu.git`，主项目以 `real_drone/dm-fc01-can-imu` 子模块引用。2026-09-27 已通过 SSH 查询该远程，访问成功，查询时没有已发布分支。

迁移范围包括固件及必要构建依赖、主机接收/USB维护工具、ROS适配、测试、README、协议/标定/构建恢复文档和验收记录。主项目仅保留必要的集成入口；ROS适配不再通过当前目录布局引用相邻源码。原厂来源和许可证继续保留在独立仓库。生成的日志、虚拟环境、固件二进制和设备参数备份保持本机artifact。

迁移验收：从新仓库独立检出，初始化其声明依赖，重新编译并运行离线检查，核对刷写/恢复工具和ROS入口，再推送并登记主项目gitlink。随后删除主项目的 `real_drone/docs/dm-fc01/PX4-Autopilot_dm-fc01`、`real_drone/can_px4` 及对应 `.gitmodules` 登记、本地子模块元数据；同时清理已迁移的重复工具和文档。当前工具仍有两个MAVLink备份辅助脚本依赖旧 `can_px4` 路径，必须先解除这些依赖。

这份计划记录于 USB 恢复阶段，当时尚未删除旧目录或发布仓库。

## 2026-09-27 整机重启后的复测

主机重启恢复了 USB。诊断版 BMI088 加速度 1600 Hz、陀螺 2000 Hz，SPI 错误、FIFO 溢出和 DRDY missed 为零。启动 CAN 后成功解码 946 个样本，连续有效段约 4.73 秒，随后停止输出；10 秒窗口平均仅 94.59 Hz，不能判为 200 Hz 连续输出通过。保存于本机 `whole-reboot-can/summary.json`。该次现场轴数据波动较大，不能作为静止噪声测试。

第五次刷写完成并校验成功，自动启动运行。新增寄存器诊断显示 PSR=0x77b（ACK error / error passive）、TXBRP=0xffffffff（发送队列全部等待）。独立模块漏调用驱动时钟初始化，导致发送超时清理不工作；源码已补 `SystemClock::instance()`，第六版已构建，尚待刷入验证。CANerr=0 原先只统计 abort/timeout，不能据此声称总线无物理错误。

第六次自动重启进入 bootloader 时，主机 tegra-xusb 再次出现 transfer-event 错误及 USB descriptor -110；该次尚未写入。正在恢复 USB 控制器并核对 CAN 供电。尚未执行电机运动。

第六版随后通过重新绑定主机 `tegra-xusb` 控制器成功枚举、刷入并校验。CAN 驱动时钟初始化修复已生效：断线时持续清理超时发送，队列不再永久锁死；CANerr 计数增加符合发送超时。用户随后确认飞控 CAN 线曾断开并已重新连接；重连后的 30 秒采集仍无 IMU 帧，电机只读 inspect 未完整通过（CAN response timeout），已请现场核对 CAN1 插口。源码和工具已复制到新独立仓库作迁移构建，旧目录未删除、远程未发布。

迁移目录独立构建通过：`make -C real_drone/dm-fc01-can-imu damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/home/airman/FC-CLAMP_Real_Drone/.omx/venvs/can-imu-build/bin/python`。新路径下 6 个 Python 协议测试、C++ ASan/UBSan 核心检查通过。ROS 新路径 colcon 构建通过；同步接收器同时运行时节点被独占锁拒绝，停止接收器后单独运行 5 秒正常（timeout 124），但无 IMU 数据。旧目录和远程尚未完成交付清理。

当前维护源码以新目录 `real_drone/dm-fc01-can-imu` 为准；板上是第六版（时钟修复），新目录另将状态输出字段从 sent 改名 queued，避免将入队误解成已送达。固件新目录全量构建通过，但该字段改名版本尚未再次烧录。厂商手册明确只有一个 CAN1 接口，四针为 1 GND、2 VBAT、3 H、4 L。先前询问 CAN2 不适用于本板。电机运动仍为零次；完整验收与最终发布/旧子模块删除待连接恢复后执行。

## 实物异常：J4 大角度转动及线缆牵拉

用户报告 J4 曾大角度转动并扯住线缆，随后已解除牵拉、检查接头并摆回 [90,0,0,0]，重新确认可标定。此前没有执行有界运动批次，但这不等于实物没有运动；先前“未执行电机运动”仅指未调用预定运动实验。异常原因未确定。需要排查电机是否误接收扩展帧：旧 IMU ID 低位包含 1–4，厂商电机协议只声明标准帧，尚无其拒绝扩展帧的实证。

已通过 USB 执行 `can_imu stop`、`param set CI_AUTOSTART 0`、`param save`，确认 not running。在问题排除之前不得恢复旧 ID 的 IMU 共线输出。四电机检查仍超时，尚未写入零点，正在逐电机只读诊断。

逐轴只读复查：J1/J2/J3 UUID 与原记录一致，位置约 1.6962/−3.6983/10.2374°，速度绝对值均小于 0.002 rpm、电流均为零。J4 UUID 查询超时。四轴标定尚未执行。源码默认 CI_AUTOSTART 改为 0，当前板已另行保存为 0；既有参数仍会覆盖新固件默认值。

## 2026-09-27 15:17 四轴参考标定完成

用户确认修复 J4 断线、整机重新上电并摆至 [90,0,0,0]。USB 检查 can_imu not running，CI_AUTOSTART=0 已保存。四轴身份与原 UUID 一致，静止检查通过。复用共享驱动 `calibrate_reference` 写入参考坐标，逐轴校验后再次完整查询通过：89.999443、0.000387、0.000222、−0.000175°。未发送运动目标，未改变增益；本次尚未通过再次断电验证标定持久化。

命令：`real_drone/aerial_arm_motor_autotune/.venv/bin/python /home/airman/.local/state/aerial-arm-motor-autotune/imu-can-20260927/calibrate_reference_run.py`（执行前同一封装 --simulate 通过）。结果保存在该目录 `reference-calibration-live-20260927_151708.json`。最初误用驱动 CLI 读取调参配置时因缺少 control 字段退出，发生在打开硬件之前；随后使用经过 validate 的调参配置及与 UI 相同的维护函数完成标定。IMU CAN 继续关闭，J4 异常运动原因及共线协议隔离尚待排查。
