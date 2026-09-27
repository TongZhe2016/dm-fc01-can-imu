from pathlib import Path
from setuptools import setup
here=Path(__file__).resolve().parent
setup(name='fc_clamp_can_imu',version='0.1.0',packages=['can_imu'],
      package_dir={'can_imu':str(here.parents[1]/'host/can_imu')},
      data_files=[('share/ament_index/resource_index/packages',['resource/fc_clamp_can_imu']),
                  ('share/fc_clamp_can_imu',['package.xml']),
                  ('share/fc_clamp_can_imu/launch',['launch/imu.launch.py'])],
      entry_points={'console_scripts':['can_imu_node=can_imu.ros_node:main']})
