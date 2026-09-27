# CAN IMU v1 Protocol

The device uses classic CAN at 1 Mbit/s with 11-bit standard data frames and fixed IDs for a single node. Assign IDs consistently when multiple nodes share a bus.

## Messages and Fragmentation

| Range | Contents | Direction |
|---|---|---|
| 0x680–687 | Six-axis sample, 8 fragments | Board → host |
| 0x6A0–6A7 | Status, 8 fragments, 1 Hz | Board → host |
| 0x6C0 | Synchronization request, DLC 2, little-endian sequence number | Host → board |
| 0x6B0–6B3 | Synchronization reply, 4 fragments | Board → host |

Data, status, and synchronization replies use DLC 8 fragments: the first 2 bytes contain the sequence number and the remaining 6 bytes contain payload. The low 3 bits of the CAN ID identify the fragment index.

The last 2 bytes of each complete packet contain CRC16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection, and no final XOR. The CRC covers, in order, the little-endian uint32 baseID, the little-endian uint16 sequence number, and every packet byte except the CRC itself. The standalone CRC of `123456789` is 0x29B1.

## Six-Axis Sample

Each sample is 48 bytes. All multibyte fields are little-endian:

| Offset | Type | Contents |
|---|---|---|
| 0 | uint64 | Window-end time in microseconds since boot |
| 8 | float32[3] | Acceleration specific force, m/s², preserving the gravity response |
| 20 | float32[3] | Angular velocity, rad/s |
| 32 | uint32 | boot/session |
| 36 | uint16 | Window duration, 5000 us |
| 38 | uint16 | flags |
| 40 | int16 | Temperature × 100; −32768 means unavailable |
| 42 | uint8 | Source: 0 BMI088, 1 ICM45686 |
| 43 | uint8 | Protocol version 1 |
| 44 | uint16 | Configuration epoch |
| 46 | uint16 | CRC |

`flags` are defined as follows:

| Bit | Meaning |
|---|---|
| bit0 | Sample is complete and valid |
| bit1 | Both accelerometer and gyroscope calibrations are valid |
| bit2 | Temperature has stayed within ±1°C of the target for 10 consecutive seconds |
| bit3 | The window used polling or unknown timestamps |
| bit4 | Amplitude clipping occurred; bit0 is also cleared |
| bit5 | The selected heater has a fault |

Time-quality flags propagate through the processing chain, including low-pass filtering and interpolation.

## Status and Clock Synchronization

Status is 48 bytes: `uint64 timestamp; uint32 boot, tx_drop, incomplete, can_errors, orb_lost, gaps_bmi, gaps_icm; int16 temperatures[2]; uint8 source,version; uint16 epoch; uint8 heater_fault_mask,calibrated_mask; uint16 crc`.

A synchronization reply is 24 bytes: `uint64 t2_us,t3_us; uint32 boot; uint16 version,crc`. The device accepts requests at up to 20 Hz; the host requests synchronization at 10 Hz by default.

The four synchronization timestamps are:

| Timestamp | Recorded at |
|---|---|
| t1 | Host sends the request |
| t2 | Device low-level driver dequeues the request, converted to the hrt clock |
| t3 | Device is about to send the reply fragments |
| t4 | Host receives the complete reply |

All four are software timestamps. The host selects low-RTT samples to fit an affine clock mapping. The mapping expires after 1 second without successful synchronization. Absolute synchronization accuracy requires independent measurement.

## CRC Reference Vector

Cross-language CRC vector with baseID=0x680, seq=1, timestamp=5000, a=(1,2,3), g=(0.1,0.2,0.3), boot=1, dt=5000, flags=1, T=48, source=0, version=1, and epoch=1:

```text
88130000000000000000803f0000004000004040cdcccc3dcdcc4c3e9a99993e0100000088130100c0120001010020bf
```

## Reassembly and Sessions

The receiver retains at most 32 incomplete packets, with a 20 ms reassembly timeout. Duplicate fragments are ignored; conflicting fragments or CRC failures cause the entire packet to be discarded. Only samples with increasing timestamps in the current session are delivered. Sequence numbers may wrap around.

A device reset or stream session change clears reassembly state; a ROS configuration change clears the clock mapping. The boot counter is stored in parameters. Restoring old parameters or manually resetting the counter may reuse a historical session, in which case the receiver process must also be restarted.

## Transmission and Bandwidth

Fragments have a 3 ms transmission deadline, and the low-level driver has 32 hardware transmit slots. Expired-frame drops count as low-level CAN errors; enqueue failures count toward `tx_drop`. The receiver discards incomplete packets after the reassembly timeout.

The budget for the 200 Hz primary stream is 8×200×135≈21.6%. Total load also includes status and synchronization traffic. Standard motor IDs 1–4 have higher arbitration priority than the IMU; motor parameter ID 0x7FF has lower priority. Shared-bus validation must also check parameter-transaction latency.

The receiver accepts only the standard data frames listed above, filtering out extended frames, RTR frames, and error frames. The message layout version is 1; CRC calculation must use this version's standard-frame baseID.
