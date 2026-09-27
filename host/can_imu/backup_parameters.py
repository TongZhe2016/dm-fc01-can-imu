#!/usr/bin/env python3
"""Read full-precision factory MAVLink parameters before replacing the application."""
import argparse
import fcntl
import json
from pathlib import Path
import sys
import time
import struct
from maintenance import hardware_lock
from pymavlink import mavutil
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
with hardware_lock().open('a') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 m=mavutil.mavlink_connection('/dev/serial/by-id/usb-DAMIAO_DAMIAO_DM-FC01_0-if00',baud=115200)
 if not m.wait_heartbeat(timeout=5):raise RuntimeError('No heartbeat')
 m.mav.param_request_list_send(m.target_system,m.target_component)
 records={}; count=None;deadline=time.monotonic()+60
 while time.monotonic()<deadline:
  v=m.recv_match(type='PARAM_VALUE',blocking=True,timeout=.5)
  if v:
   if v.param_id=='_HASH_CHECK' or v.param_index>=v.param_count:continue
   count=v.param_count
   # PX4 uses MAVLink bytewise parameter encoding, including integer bit patterns.
   raw=struct.pack('<f',v.param_value)
   value=struct.unpack('<i' if v.param_type==6 else '<I',raw)[0] if v.param_type in (5,6) else v.param_value
   records[v.param_index]=dict(name=v.param_id,value=value,type=v.param_type,index=v.param_index)
   if len(records)==count:break
  elif count:
   for i in range(count):
    if i not in records:m.mav.param_request_read_send(m.target_system,m.target_component,b'',i)
 a.output.write_text(json.dumps(dict(count=count,received=len(records),parameters=[records[i] for i in sorted(records)]),indent=2)+'\n')
 m.close()
 if count!=len(records):raise RuntimeError(f'Incomplete backup {len(records)}/{count}')
 print(f'Backed up all {count} parameters to {a.output}')
