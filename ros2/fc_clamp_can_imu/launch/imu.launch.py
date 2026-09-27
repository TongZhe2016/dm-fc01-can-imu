from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('interface',default_value='can0'),
        Node(package='fc_clamp_can_imu',executable='can_imu_node',output='screen',
             parameters=[{'interface':LaunchConfiguration('interface'),'frame_id':'ee_imu'}])])
