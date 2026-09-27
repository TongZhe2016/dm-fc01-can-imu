# 构建与恢复

工作目录为独立固件仓库根目录。构建目标 `damiao_dm-fc01_imu`，应用 board ID 7140。必要子模块与工具版本见根 README。

```bash
make damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/path/to/build-env/bin/python
python3 host/can_imu/flash.py build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4
python3 host/can_imu/usb_query.py --plain 'ver all' 'can_imu status'
```

刷写仅更新应用，不更新 bootloader。已验证软件从 USB NSH 进入现有 bootloader、擦除、写入和校验；无需按 Boot。控制台不可用时可在 uploader 等待期间用 RESET/完整断电上电进入启动窗口。USB 供电仍连接时，仅切断其他电源可能不构成完整 MCU 断电。

## 本次主机 USB 枚举故障

Jetson 主机在软件重启设备时曾报 `tegra-xusb: Transfer event TRB DMA ptr not part of current TD`，随后 descriptor read/64 返回 -110。此时应用串口和 bootloader 都无法枚举；不能仅据此判定固件崩溃。主机重启可恢复；本次也成功通过解绑/重绑主机 `3610000.usb` 的 `tegra-xusb` 驱动恢复并完成刷写。

这个操作会断开该控制器上全部 USB 外设，属于主机维护操作，不由刷写工具自动执行。现场此次控制器上仅有飞控、USB hub 和蓝牙；不能将同一 sysfs 路径和影响范围直接用于其他电脑。先检查 `lsusb`、控制器拓扑和内核日志。

刷写程序有 120 秒单次超时，等待设备最多 1800 秒；消失时不会无限后台刷写。失败后查看日志中的 Program/Verify 是否完成，再决定恢复步骤。

## CAN 诊断

使用 CAN1、经典 CAN 1 Mbps。电机和 IMU 均使用标准帧，ID 分配见 [PROTOCOL.md](PROTOCOL.md)。状态中的 `queued` 是完整包入队计数，不能作为总线发送成功计数。`tx_drop` 是入队失败；`CANerr` 包含底层发送超时/abort 事件。寄存器 CCCR、PSR、TXFQS、TXBRP、TXBTO、TXBCF、IR、IE 用于诊断实际控制器状态。

本次断线时 PSR=0x77b，最后错误为 ACK error；发送排队并不能证明线路连通。独立模块显式初始化驱动时钟，保证 3 ms 发送截止时间生效，过期帧清理后仍可尝试后续新数据。接收器应报告整个观察窗口的初始/末尾静默时间，不仅报告有数据片段的平均间隔。

参考原厂应用和出厂参数备份保留在主项目本机 artifact/硬件文档中，不作为新固件生成物提交。恢复旧参数可能复用 boot/session，恢复后同时重启主机接收器。
