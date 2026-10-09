"""Publication checks without opening a CAN socket or starting a ROS node."""
from types import SimpleNamespace
import unittest

from can_imu.ros_node import CanImuNode


class PublicationTests(unittest.TestCase):
    def publish(self, age, *, mapped=True, flags=7):
        raw, data = [], []
        now = 100.
        node = SimpleNamespace(
            identity=(1, 1, 0), clock_pair=None,
            get_clock=lambda: SimpleNamespace(now=lambda: SimpleNamespace(nanoseconds=100_000_000_000)),
            get_parameter=lambda key: SimpleNamespace(value='ee_imu' if key == 'frame_id' else True),
            mapping=SimpleNamespace(map=lambda *_: now-age if mapped else None),
            raw=SimpleNamespace(publish=raw.append), pub=SimpleNamespace(publish=data.append),
        )
        event=dict(boot=1, epoch=1, source=0, device_us=1, flags=flags,
                   accel=[0., 0., 9.8], gyro=[0., 0., 0.])
        CanImuNode.publish_sample(node, event, now)
        return node.reason, raw, data

    def test_delayed_samples_keep_sample_timestamp(self):
        for age in (.005, .025, .125, .300):
            with self.subTest(age=age):
                reason, raw, data = self.publish(age)
                self.assertEqual(reason, 'ready')
                self.assertEqual(len(data), 1)
                self.assertEqual(raw, data)
                stamp = data[0].header.stamp
                self.assertAlmostEqual(stamp.sec+stamp.nanosec*1e-9, 100.-age, places=7)

    def test_future_timestamp_still_rejected(self):
        reason, raw, data = self.publish(-.003)
        self.assertEqual(reason, 'sample_age_out_of_range')
        self.assertEqual(len(raw), 1)
        self.assertEqual(data, [])

    def test_unmapped_timestamp_still_rejected(self):
        reason, raw, data = self.publish(.005, mapped=False)
        self.assertEqual(reason, 'time_sync_not_ready')
        self.assertEqual(len(raw), 1)
        self.assertEqual(data, [])

    def test_invalid_timestamp_still_rejected(self):
        for flags in (6, 15, 23):
            with self.subTest(flags=flags):
                reason, raw, data = self.publish(.005, flags=flags)
                self.assertEqual(reason, 'invalid_or_diagnostic_timestamp')
                self.assertEqual(len(raw), 1)
                self.assertEqual(data, [])


if __name__ == '__main__':
    unittest.main()
