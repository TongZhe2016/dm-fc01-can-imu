# CAN IMU v1

经典 CAN，1 Mbit/s，11 位标准帧，单节点固定 ID。

| 范围 | 内容 | 方向 |
|---|---|---|
| 0x680–687 | 六轴样本，8片 | 板→主机 |
| 0x6A0–6A7 | 状态，8片，1 Hz | 板→主机 |
| 0x6C0 | 对时请求，DLC2，小端序号 | 主机→板 |
| 0x6B0–6B3 | 对时回复，4片 | 板→主机 |

回复和数据的每片 DLC8：2字节序号 +6字节数据。低3位为分片索引。最后两个字节为 CRC16/CCITT-FALSE：多项式0x1021、初值0xFFFF、无反射、无末异或；覆盖小端 baseID uint32、序号uint16、整包除CRC外的全部字节。`123456789` 的独立CRC为0x29B1。

样本48字节，全小端：

| 偏移 | 类型 | 内容 |
|---|---|---|
| 0 | uint64 | 窗口末端启动微秒 |
| 8 | float32[3] | 加速度比力，m/s²，保留重力响应 |
| 20 | float32[3] | 角速度，rad/s |
| 32 | uint32 | boot/session |
| 36 | uint16 | 窗口长度5000us |
| 38 | uint16 | flags |
| 40 | int16 | 温度×100；−32768不可用 |
| 42 | uint8 | 源0BMI088、1ICM45686 |
| 43 | uint8 | 协议版本1 |
| 44 | uint16 | 配置epoch |
| 46 | uint16 | CRC |

flags：bit0完整有效；bit1加速度和陀螺标定均有效；bit2目标温度±1°C连续10秒；bit3窗口使用了轮询/未知时间戳；bit4裁剪（同时清bit0）；bit5所选加热器故障。低通和插值不会使轮询时间戳变成DRDY时间戳。

状态48字节：`uint64 timestamp; uint32 boot, tx_drop, incomplete, can_errors, orb_lost, gaps_bmi, gaps_icm; int16 temperatures[2]; uint8 source,version; uint16 epoch; uint8 heater_fault_mask,calibrated_mask; uint16 crc`。

对时24字节：`uint64 t2_us,t3_us; uint32 boot; uint16 version,crc`。请求最多接受20Hz，主机默认10Hz。t2为底层驱动出队时刻换算到hrt时钟，t3为发送分片前时刻；两者均为软件打点。主机保留t1发送和t4收齐，按低RTT样本拟合仿射时钟。1秒无成功同步即过期。

CRC跨语言向量，baseID=0x680、seq=1，timestamp=5000，a=(1,2,3)，g=(0.1,0.2,0.3)，boot=1，dt=5000，flags=1，T=48，source=0，version=1，epoch=1：

```text
88130000000000000000803f0000004000004040cdcccc3dcdcc4c3e9a99993e0100000088130100c0120001010020bf
```

重组最多32个未完成包，超时20ms；重复片不重复交付，冲突片放弃本包；CRC失败不交付。仅交付当前会话中时间递增的样本，序号可回绕。设备复位/流会话变化后清空重组；ROS配置变化清空对时映射。boot计数依赖参数存储，恢复旧参数或人为重置计数可能复用历史会话，应同时重启接收进程。

发送分片截止时间3ms，队列由底层32个硬件发送槽界定。过期丢弃计入底层CAN错误；入队失败计入tx_drop。接收端超时丢弃部分包。主流预算8×200×135≈21.6%，加状态/对时。标准电机ID1–4优先于本标准ID；电机参数标准ID0x7FF的优先级低于本数据流。参数事务延迟必须纳入共线验收。

本版使用经典 CAN 标准帧，明确拒绝扩展帧、RTR 和错误帧。预留上述 ID 给 IMU，电机 ID 保持 1–4，配置 ID 为 0x7FF；后续添加节点时应统一登记 ID。报文布局版本仍为 1，CRC 包含新的 baseID。旧扩展 ID 试验流已停用。
