"""ROS 2 adapter for the dedicated CAN IMU. No motor or flight-controller commands."""
import fcntl
import json
import socket
import struct
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Imu
from std_msgs.msg import String
from .protocol import SYNC_REQUEST, FRAME, Decoder, ClockMap, socket_filters


class CanImuNode(Node):
    def __init__(self):
        super().__init__('ee_can_imu')
        for key,value in dict(interface='can0',frame_id='ee_imu',require_calibration=True,require_warm=False).items():
            self.declare_parameter(key,value)
        iface=self.get_parameter('interface').value
        self.lock=open('/tmp/dm-fc01-imu-sync-'+iface+'.lock','a')
        fcntl.flock(self.lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        self.sock=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW)
        self.sock.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1<<20)
        self.sock.setsockopt(socket.SOL_CAN_RAW,socket.CAN_RAW_FILTER,socket_filters())
        self.sock.bind((iface,));self.sock.setblocking(False)
        self.decoder=Decoder();self.mapping=ClockMap();self.requests={};self.seq=0
        self.last_rx=None;self.status=None;self.identity=None;self.clock_pair=None;self.reason='waiting_for_data'
        self.raw=self.create_publisher(Imu,'/ee_imu/raw',qos_profile_sensor_data)
        self.pub=self.create_publisher(Imu,'/ee_imu/data',qos_profile_sensor_data)
        self.diag=self.create_publisher(String,'/ee_imu/diagnostics',10)
        self.create_timer(.002,self.receive);self.create_timer(.1,self.sync);self.create_timer(1.,self.diagnostics)

    def sync(self):
        now=time.monotonic();self.seq=(self.seq+1)&65535
        try:
            self.sock.send(FRAME.pack(SYNC_REQUEST,2,struct.pack('<H',self.seq)))
            self.requests[self.seq]=now
        except BlockingIOError:pass
        self.requests={k:v for k,v in self.requests.items() if now-v<1}

    def receive(self):
        for _ in range(256):
            try:raw=self.sock.recv(16)
            except BlockingIOError:break
            if len(raw)!=16:continue
            ident,dlc,data=FRAME.unpack(raw);now=time.monotonic()
            event=self.decoder.feed(ident,data[:dlc],now)
            if not event:continue
            if event['kind']=='sync':
                sent=self.requests.pop(event['seq'],None)
                if sent is not None:self.mapping.update(sent,event)
            elif event['kind']=='status':self.status=event
            else:self.publish_sample(event,now)

    def publish_sample(self,event,now):
        self.last_rx=now
        ros_ns=self.get_clock().now().nanoseconds
        identity=event['boot'],event['epoch'],event['source']
        if self.identity is not None and self.identity!=identity:
            self.mapping=ClockMap()
        self.identity=identity
        if self.clock_pair is not None:
            old_mono,old_ros=self.clock_pair
            if abs((ros_ns-old_ros)*1e-9-(now-old_mono))>.05:self.mapping=ClockMap()
        self.clock_pair=now,ros_ns
        mapped=self.mapping.map(event['device_us'],event['boot'],now)
        msg=Imu();msg.header.frame_id=self.get_parameter('frame_id').value
        stamp=ros_ns if mapped is None else ros_ns+int((mapped-now)*1e9)
        msg.header.stamp.sec=stamp//1000000000;msg.header.stamp.nanosec=stamp%1000000000
        msg.orientation_covariance[0]=-1.
        msg.linear_acceleration.x,msg.linear_acceleration.y,msg.linear_acceleration.z=event['accel']
        msg.angular_velocity.x,msg.angular_velocity.y,msg.angular_velocity.z=event['gyro']
        # Zero covariance denotes unknown, not a measured covariance estimate.
        self.raw.publish(msg)
        flags=event['flags']
        if not flags&1 or flags&(8|16):self.reason='invalid_or_diagnostic_timestamp'
        elif mapped is None:self.reason='time_sync_not_ready'
        # Preserve sample time; consumers own their input-age budgets.
        elif now-mapped < -.002:self.reason='sample_age_out_of_range'
        elif self.get_parameter('require_calibration').value and not flags&2:self.reason='calibration_required'
        elif self.get_parameter('require_warm').value and not flags&4:self.reason='temperature_not_stable'
        else:self.reason='ready';self.pub.publish(msg)

    def diagnostics(self):
        if self.last_rx is None or time.monotonic()-self.last_rx>.1:self.reason='stream_timeout'
        msg=String();msg.data=json.dumps(dict(state=self.reason,identity=self.identity,counts=dict(self.decoder.counts),status=self.status))
        self.diag.publish(msg)

    def destroy_node(self):
        self.sock.close();self.lock.close();super().destroy_node()


def main():
    rclpy.init();node=None
    try:
        node=CanImuNode();rclpy.spin(node)
    except KeyboardInterrupt:pass
    finally:
        if node:node.destroy_node()
        if rclpy.ok():rclpy.shutdown()
