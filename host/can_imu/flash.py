#!/usr/bin/env python3
"""Wait a bounded time for DM-FC01 USB, then flash one checked application image."""
import argparse
import fcntl
import json
from pathlib import Path
import subprocess
import sys
import time
from maintenance import ROOT, hardware_lock


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('firmware',type=Path)
    p.add_argument('--port',default='/dev/serial/by-id/usb-DAMIAO_DAMIAO_DM-FC01_0-if00')
    p.add_argument('--wait',type=float,default=60)
    args=p.parse_args()
    if not 0<args.wait<=1800:p.error('wait must be 0..1800 seconds')
    image=json.loads(args.firmware.read_text())
    if image.get('board_id')!=7140 or image.get('magic')!='PX4FWv1':p.error('not a DM-FC01 application package')
    root=ROOT
    uploader=root/'Tools/px_uploader.py'
    lockpath=hardware_lock()
    print(f'Waiting up to {args.wait:g}s for {args.port}; image {args.firmware.resolve()}',flush=True)
    end=time.monotonic()+args.wait
    with lockpath.open('a') as lock:
        while time.monotonic()<end:
            if Path(args.port).exists():
                try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                except BlockingIOError:pass
                else:
                    print('USB detected and hardware lock acquired; starting one upload',flush=True)
                    try:
                        return subprocess.run([sys.executable,'-u',str(uploader),'--port',args.port,str(args.firmware.resolve())],timeout=120).returncode
                    except subprocess.TimeoutExpired:
                        print('Upload timed out; inspect board state before retry',flush=True);return 2
                    finally:fcntl.flock(lock,fcntl.LOCK_UN)
            time.sleep(.1)
    print('No upload performed: USB/lock did not become available',flush=True);return 2

if __name__=='__main__':raise SystemExit(main())
