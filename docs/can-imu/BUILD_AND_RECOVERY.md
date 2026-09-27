# Build and Recovery

Run commands from the standalone firmware repository root. The build target is `damiao_dm-fc01_imu`, with application board ID 7140. See the [README](../../README.md) for dependencies and tool versions.

```bash
make damiao_dm-fc01_imu -j4 PYTHON_EXECUTABLE=/path/to/build-env/bin/python
python3 host/can_imu/flash.py build/damiao_dm-fc01_imu/damiao_dm-fc01_imu.px4
python3 host/can_imu/usb_query.py --plain 'ver all' 'can_imu status'
```

The flashing tool uses USB NSH to reboot the application into the existing bootloader, then erases, writes, and verifies the application. This procedure has been tested on hardware, requires no Boot-button press, and preserves the bootloader version. If the console is unavailable, use RESET or a full power cycle while the uploader is waiting to catch the bootloader window. Disconnecting other power sources while USB power remains connected may not fully power down the MCU.

## USB Enumeration Failures

When rebooting the device in software, the Jetson host once reported `tegra-xusb: Transfer event TRB DMA ptr not part of current TD`, followed by descriptor read/64 returning -110. Neither the application serial port nor the bootloader could enumerate. Check the host USB controller state first in this situation. Rebooting the host can restore operation. In this incident, unbinding and rebinding the host's `tegra-xusb` driver for `3610000.usb` also restored enumeration and allowed flashing to complete.

Rebinding the controller disconnects all USB peripherals attached to it and must be performed manually. Before doing so, check `lsusb`, the controller topology, and kernel logs to confirm the local sysfs path and affected devices. In this incident, the controller hosted the flight controller, a USB hub, and Bluetooth.

The flashing program allows 120 seconds per upload attempt and up to 1800 seconds waiting for a device. After a failure, check whether Program/Verify completed in the log before choosing recovery steps.

## CAN Diagnostics

Use CAN1 with classic CAN at 1 Mbps. Both motors and the IMU use standard frames; see [PROTOCOL.md](PROTOCOL.md) for ID allocation. `queued` counts complete packets enqueued; actual delivery is confirmed by receiver sequence and CRC checks. `tx_drop` counts enqueue failures. `CANerr` includes low-level transmission timeout/abort events. Registers CCCR, PSR, TXFQS, TXBRP, TXBTO, TXBCF, IR, and IE expose the actual controller state for diagnostics.

One disconnected-bus record showed PSR=0x77b, with ACK error as the last error, indicating that transmissions received no ACK. The standalone module explicitly initializes the driver clock so that the 3 ms transmission deadline takes effect. After expired frames are cleared, it can continue attempting to send new data. The receiver reports initial silence, trailing silence, and valid-stream intervals to identify interruptions.

The vendor reference application and factory parameter backups are kept in local records and hardware documentation in the parent project. Restoring old parameters may reuse a boot/session value; restart the host receiver after restoration.

## Recovery Files on the Validation Host

The USB recovery image is `.omx/artifacts/dm-fc01-imu/usb-rescue.px4`; it provides only USB maintenance after startup. The vendor reference application is at `real_drone/docs/dm-fc01/固件/PX4/damiao_dm-fc01_V1.16.px4` in the parent project; its Git hash differs from the application originally installed on the board. Factory parameter backups are `factory-parameters.json` and `preflash.json` in the local records directory.
