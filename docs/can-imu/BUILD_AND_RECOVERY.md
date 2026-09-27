# 构建与恢复

工作目录为独立固件仓库根目录。构建目标 `damiao_dm-fc01_imu`，应用 board ID 7140。依赖与工具版本见 [README](../../README.md)。

```bash
make damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/path/to/build-env/bin/python
python3 host/can_imu/flash.py build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4
python3 host/can_imu/usb_query.py --plain 'ver all' 'can_imu status'
```

刷写工具通过 USB NSH 让应用重启进入现有 bootloader，完成擦除、写入和校验。这个流程已实测，无需按 Boot 键，bootloader 保持原版本。控制台不可用时可在 uploader 等待期间用 RESET/完整断电上电进入启动窗口。USB 供电仍连接时，仅切断其他电源可能不构成完整 MCU 断电。

## USB 枚举故障

Jetson 主机在软件重启设备时曾报 `tegra-xusb: Transfer event TRB DMA ptr not part of current TD`，随后 descriptor read/64 返回 -110。此时应用串口和 bootloader 都无法枚举，应先检查主机 USB 控制器状态。主机重启可恢复；本次也成功通过解绑/重绑主机 `3610000.usb` 的 `tegra-xusb` 驱动恢复并完成刷写。

重新绑定控制器会断开其全部 USB 外设，需要手动执行。操作前检查 `lsusb`、控制器拓扑和内核日志，确认本机 sysfs 路径及受影响设备。该次现场控制器上连接了飞控、USB hub 和蓝牙。

刷写程序单次上传超时为 120 秒，等待设备的上限为 1800 秒。失败后查看日志中的 Program/Verify 是否完成，再决定恢复步骤。

## CAN 诊断

使用 CAN1、经典 CAN 1 Mbps。电机和 IMU 均使用标准帧，ID 分配见 [PROTOCOL.md](PROTOCOL.md)。`queued` 统计完整包入队次数，实际送达情况由接收端序列和 CRC 检查确认。`tx_drop` 是入队失败；`CANerr` 包含底层发送超时/abort 事件。寄存器 CCCR、PSR、TXFQS、TXBRP、TXBTO、TXBCF、IR、IE 用于诊断实际控制器状态。

一次断线记录中的 PSR=0x77b，最后错误为 ACK error，表明发送未收到 ACK。独立模块显式初始化驱动时钟，保证 3 ms 发送截止时间生效，过期帧清理后仍可尝试后续新数据。接收器同时报告初始静默、末尾静默和有效流间隔，用于识别断流。

原厂参考应用和出厂参数备份保存在主项目的本机记录及硬件文档中。恢复旧参数可能复用 boot/session，恢复后同时重启主机接收器。

## 验收主机的恢复文件

USB 恢复镜像为 `.omx/artifacts/dm-fc01-imu/usb-rescue.px4`，启动后仅提供 USB 维护。原厂参考应用位于主项目 `real_drone/docs/dm-fc01/固件/PX4/damiao_dm-fc01_V1.16.px4`，其 git hash 与最初板上应用不同。出厂参数备份为本机记录目录中的 `factory-parameters.json` 和 `preflash.json`。
