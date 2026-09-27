#!/usr/bin/env python3
"""Bounded USB maintenance queries. Hold the repository hardware lock."""
import argparse, fcntl, json, os, sys, time
from pathlib import Path
from maintenance import ROOT, hardware_lock

def main():
 p=argparse.ArgumentParser();p.add_argument('commands',nargs='+');p.add_argument('--plain',action='store_true');p.add_argument('--timeout',type=float,default=2);p.add_argument('--output');p.add_argument('--port',default='/dev/serial/by-id/usb-DAMIAO_DAMIAO_DM-FC01_0-if00');a=p.parse_args()
 results=[]
 with hardware_lock().open('a+') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  if a.plain:
   import serial
   link=serial.Serial(a.port,115200,timeout=.1,exclusive=True)
   link.write(b'\r\r\r')
   deadline=time.monotonic()+5;ready=b''
   while time.monotonic()<deadline and b'nsh>' not in ready:
    ready+=link.read(4096)
   if b'nsh>' not in ready:link.close();raise RuntimeError('USB NSH prompt unavailable')
  else:
   from pymavlink import mavutil
   link=mavutil.mavlink_connection(a.port,baud=115200,source_system=245,source_component=190)
   if link.wait_heartbeat(timeout=5) is None:link.close();raise RuntimeError('No heartbeat')
  try:
   for cmd in a.commands:
    b=('\n'+cmd+'\n').encode()
    if a.plain:link.write(b)
    else:
     if len(b)>70:raise ValueError('Command too long')
     link.mav.serial_control_send(10,6,0,0,len(b),list(b)+[0]*(70-len(b)))
    end=time.monotonic()+a.timeout;out=[]
    while time.monotonic()<end:
     if a.plain: data=link.read(4096)
     else:
      m=link.recv_match(type='SERIAL_CONTROL',blocking=True,timeout=.1)
      data=bytes(m.data[:m.count]) if m else b''
     out.append(data.decode('utf-8','replace'))
    result={'command':cmd,'output':''.join(out)};results.append(result);print(json.dumps(result,ensure_ascii=False),flush=True)
  finally:
   if not a.plain:link.mav.serial_control_send(10,0,0,0,0,[0]*70)
   link.close()
 if a.output:Path(a.output).write_text(json.dumps(results,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
