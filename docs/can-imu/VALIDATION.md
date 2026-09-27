# DM-FC01 CAN IMU 实机验收

测试日期：2026-09-27。硬件 STM32H743VI rev V，BMI088 + ICM45686，CAN1 经典 1 Mbps，四个 ENCOS 电机 ID 1–4。主机为 Jetson，原生 SocketCAN can0。固件目标 `damiao_dm-fc01_imu`，board ID 7140。

## 当前结果

- USB 应用自动重启进入 bootloader、擦除、写入、校验成功；未修改 bootloader。
- BMI088 加速度 FIFO 1600 Hz、陀螺 FIFO 2000 Hz，主流重采样至 200 Hz；共线对照期间无 SPI 传输错误、溢出、DRDY missed 或有效流缺口；后续诊断压力问题见下节。
- 标准帧状态输出与 35 秒完整流均通过关节静止监视。7009 个样本，流内 199.99965 Hz、所有 flags=1、零缺样，关节最大漂移小于 0.004°。40 秒观察窗口包含显式启动/停止静默，不能把窗口平均 175.21 Hz 当作流频率。
- 四轴参考姿态 [90,0,0,0] 已写入并读回；J4 断线修复后四轴身份核对通过。
- 两轮相同的 10 动作低速基线均 complete，0.5 rpm、每段 0.5 秒、150 Hz 控制；单轮约 30 秒运动、约 34 秒总线观察。保持原速度环 KP=0.006、KI=0.04999，结束后恢复确认，四轴电流为零。

## CAN 共线对照

| 指标 | 关闭 IMU 主流 | 200 Hz IMU 主流 |
|---|---:|---:|
| 估计总线利用率下界 | 18.67% | 35.99% |
| 保守位填充上界 | 22.59% | 43.66% |
| 电机反馈延迟 P50 | 3.3185 ms | 3.5140 ms |
| 电机反馈延迟 P99 | 4.881 ms | 5.203 ms |
| 电机反馈延迟最大值 | 11.247 ms | 10.946 ms |
| 电机反馈记录 | 16440 | 15864 |
| 过期反馈 / CAN 错误 / 安全事件 | 0 / 0 / 0 | 0 / 0 / 0 |
| IMU 样本 | — | 6805 |
| IMU 接收频率 | — | 199.9919 Hz |
| IMU 缺样 / socket 丢包 | — / 0 | 0 / 0 |
| IMU 到达间隔 P99 / 最大 | — | 6.479 / 6.864 ms |
| IMU 映射后样本年龄 P99 / 最大 | — | 6.580 / 6.965 ms |

利用率由观察到的标准帧长度和 CAN 位填充上下界计算，包含独立同步请求，不是示波器实测。关闭主流时仍有 1 Hz 状态和 10 Hz 对时。反馈延迟来自控制周期起点到驱动记录的接收时刻，包含顺序查询和主机调度，不是单帧物理传输时间。最大反馈延迟两轮都曾超过一个 6.67 ms 周期；本次无 stale，但不据此承诺硬实时截止时间。

结论：**对于已测的四电机 150 Hz 控制 + 200 Hz IMU，1 Mbps 有足够带宽余量。** 更高电机频率、额外节点和不同报文格式需要重新测试。本试验验收通信共存，不声称改善电机控制性能。

两轮相对参考姿态的最大角度分别约 J1=3.58°、J2=3.45°、J3=4.79°、J4=2.27°。测试范围还受到实测起始位置 ±4° 配置和既有高度、失联、电流、温度保护约束。

## 连续与 ROS 验证

第一轮连续采集在 1111.8 秒后停止，222362 个样本、1 个缺样；该缺口与 `top once` 的设备时刻 1314.945 秒重合，板上同时增加 FIFO 丢更新 6、窗口不完整 1、两路源缺口 1/2，CAN 错误和 socket 丢包为零。这一轮不计为零缺样通过。原模块优先级 235 低于 top 的 237，已改为 245，仍低于 SPI 工作队列的 250/253；随后 120 秒采集含 30 次 `top once`，23999 个样本、零缺样、零 FIFO 丢更新/源缺口/CAN 错误/socket 丢包，采样间隔 P99 6.148 ms、最大 10.890 ms。修复后长测结果如下。

正式版本连续采集 1800.002 秒，359995 个样本、199.9970 Hz，序列缺样 0、socket 丢包 0、CAN 错误 0、FIFO 丢更新 0、两路源缺口 0；incomplete 从初始 10 到结束仍为 10。到达间隔 P99 6.286 ms、最大 13.125 ms；映射后样本年龄 P99 6.665 ms、最大 14.129 ms。期间包含 5 次 `top once`。

这轮原计划为被动长测，但用户中途要求暂停时，对话中断使停止操作未执行，接收/IMU 对时进程继续到原定 30 分钟结束，覆盖了用户自行操作电机的时段。观察到每个电机 ID 各 164250 帧，以及 8 帧 0x7FF；采集程序没有发送电机控制命令。这是混合活动条件下的 IMU 连续性记录，不是固定电机控制负载试验，也不是静态噪声测量；带宽结论采用前述两轮受控对照。该暂停执行偏差已向用户说明。

双传感器和 200 Hz CAN 同时运行时，`top once` 的一次快照显示 CPU idle 59.11%、IMU 模块约 4.96%、SPI1 约 13.77%、SPI4 约 21.62%；这不是最坏情况执行时间测量。

ROS 包独立构建和无数据 smoke 已通过。真实流测试发现并修复了 Linux SocketCAN 掩码包含 CAN_ERR_FLAG 而被分配到错误帧接收列表的问题；CLI/ROS 共用标准帧掩码 `0xC00007F8`。过滤模式 10 秒收到 2000 个样本、零缺样；ROS 默认标定门控 20 秒收到 raw=3986、data=0，状态 calibration_required，消息元数据正确。显式 `require_calibration:=false` 的纯传输复测 20 秒收到 raw=3929、data=3928，节点解码零缺样，状态 ready；订阅端样本年龄 P99 8.183 ms、最大 9.051 ms。DDS 使用 best-effort，订阅数量受发现时间和交付影响，不能把它当作 CAN 缺样数。

用户结束自行调试并授权继续后，另完成 60.000 秒只接收/对时复核：11999 个样本、199.9827 Hz、零序列缺样/socket 丢包/CAN 错误/FIFO 丢更新，incomplete 保持 10；样本年龄 P99 6.220 ms、最大 10.359 ms。USB 读回版本不变，BMI088 加速度/陀螺均无 bad transfer、FIFO overflow、DRDY missed。测试结束后采集/对时进程正常退出，板上保持自启主流。

正式刷入固件提交 `ccb8a2ceb839e991c985c3d62bfbfed263eebca6`，`.px4` SHA-256 `771b89faef9085256b17f0a5186c3e3c6e985dfd87aa7bf2dd45e3a9cbe22dea`。已持久化 `CI_AUTOSTART=1`，刷写重启后自动启动，USB 读回版本和参数一致。自动启动后首次读取时 incomplete=10，具体启动瞬态未完整抓取；后续采集以此基线观察增量，不把累计值宣称为零；boot=27。ICM 轮询时间回退仍存在，保持诊断用途。

## 修复与限制

修复了厂商 H7 CAN 过滤器地址错误和初始化等待问题，补充 CAN 关闭生命周期及独立驱动时钟初始化，使断线发送截止时间生效。共享电机驱动增加 ENCOS ID 范围过滤；此前会把 IMU 0x6A0 标准帧误报为“电机 1696 故障”。回归测试确认其他设备帧被忽略，真正电机故障仍停止。

现场曾发生 J4 异常转动并拉断线缆（用户最初称 J3，后更正）。转动过程未被观察，原因未确定；不能认定由 IMU 扩展帧导致。旧扩展 ID 试验流停用，当前标准帧通过上述静止和有界运动共线测试。早期过程见 [BRINGUP_HISTORY.md](BRINGUP_HISTORY.md)。

以下不在本次已通过范围：物理六面/逐轴方向及动捕外参验收、完整加速度/陀螺标定、绝对时钟同步精度、滤波动态延迟测量、加热器热测试/故障注入、ICM 精确 FIFO 时间恢复及双 IMU 融合、EE EKF 集成。主流标定标记仍为未标定，ROS 默认 `/ee_imu/data` 受标定门控；`/ee_imu/raw` 提供诊断数据。IMU 安装倾斜不被自动校正，重力响应保留。

## 可重现检查

独立仓库根目录执行：

```bash
PYTHONPATH=host/can_imu python3 -m unittest discover -s host/can_imu/tests -v
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/modules/can_imu host/can_imu/tests/core_test.cpp -o /tmp/can-imu-core-test
/tmp/can-imu-core-test
python3 -m compileall -q host/can_imu ros2/fc_clamp_can_imu
bash -n ROMFS/can_imu/init.d/rcS
make damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/path/to/build-env/bin/python
```

结果：7 个协议测试通过，C++ ASan/UBSan 核心检查通过，独立固件全量编译通过。共享电机驱动重建后 9 个原生传输测试通过，包括共线帧误报回归。模拟动作流程完成；模拟低速响应不能用于调参评分，实机两轮基线本身均完成。

原始记录保留本机，不提交设备参数或大日志：

- `.omx/artifacts/dm-fc01-imu/standard-stream-guarded/`、`standard-endurance-30min/`（修复前）、`priority-diagnostic-stress/`、`release-endurance-30min/`（修复后）、`resumed-final-check/`。
- 同目录 `flash-release.log`、`release-ccb8a2ceb8.px4`、`release-ccb8a2ceb8.manifest.json`、`release-parameters.json`、`ros-gated-result.json`、`ros-transport-result.json`。
- `~/.local/state/aerial-arm-motor-autotune/imu-can-20260927/motor-off-20260927_153000/`。
- 同目录 `motor-on-20260927_153139/`、`can-comparison.json`、`reference-calibration-live-20260927_151708.json`、`session.json`。

运动使用主项目 `agent.sh baseline --live --fixture-ready --anchors ... --config bounded-repaired.yaml --max-evaluations 1 --max-duration-s 300 --log-frames`，精确命令在每轮 `command.json`。采集使用 `host/can_imu/receive.py --sync --observe-bus`。全局累积 RX dropped 不作为本轮丢包证据，使用采集 socket 溢出和序列缺样计数。
