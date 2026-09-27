#!/usr/bin/env python3
"""Receive/record the IMU, optionally run whitelisted time sync and observe bus load."""
import argparse
import collections
import fcntl
import json
import math
from pathlib import Path
import select
import socket
import statistics
import struct
import time
from protocol import DATA, STATUS, SYNC_REPLY, SYNC_REQUEST, EFF, ERR, FRAME, Decoder, ClockMap


def percentile(values, q):
    return sorted(values)[min(len(values)-1,int(q*len(values)))] if values else None


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--interface',default='can0')
    p.add_argument('--duration',type=float,default=30)
    p.add_argument('--sync',action='store_true')
    p.add_argument('--observe-bus',action='store_true')
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if not 0<args.duration<=7200: p.error('duration must be 0..7200 seconds')
    args.output.mkdir(parents=True,exist_ok=True)
    lock=None
    if args.sync:
        lock=open('/tmp/dm-fc01-imu-sync-'+args.interface+'.lock','a')
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    s=socket.socket(socket.AF_CAN,socket.SOCK_RAW,socket.CAN_RAW)
    s.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1<<20)
    # Count socket queue drops independently of cumulative interface statistics.
    s.setsockopt(socket.SOL_SOCKET,40,1)  # SO_RXQ_OVFL
    if not args.observe_bus:
        s.setsockopt(socket.SOL_CAN_RAW,socket.CAN_RAW_FILTER,b''.join(struct.pack('=II',base,0xFFFFFFF8) for base in (DATA,STATUS,SYNC_REPLY)))
    s.bind((args.interface,)); s.setblocking(False)
    decoder,clock=Decoder(),ClockMap()
    frames=collections.Counter(); bits_min=bits_max=0; pending={}; sync_seq=0
    start=time.monotonic(); next_sync=start; first=None; last=None; sync_sent=0; intervals=[]; ages=[]; samples=0; status=None; first_device=None; last_device=None; drops=0
    axes=[[] for _ in range(6)]
    with (args.output/'samples.jsonl').open('w') as log:
        try:
            while time.monotonic()-start<args.duration:
                now=time.monotonic()
                if args.sync and now>=next_sync:
                    sync_seq=(sync_seq+1)&65535
                    try:
                        s.send(FRAME.pack(SYNC_REQUEST,2,struct.pack('<H',sync_seq)))
                        pending[sync_seq]=now
                        sync_sent+=1
                        bits_min+=63; bits_max+=75
                    except BlockingIOError: pass
                    next_sync=now+.1
                    pending={k:v for k,v in pending.items() if now-v<1}
                if not select.select([s],[],[],.01)[0]: continue
                raw,anc,_,_=s.recvmsg(16,64); now=time.monotonic()
                if len(raw)!=16: continue
                ident,dlc,data=FRAME.unpack(raw); data=data[:dlc]
                for level,kind,value in anc:
                    if level==socket.SOL_SOCKET and kind==40: drops=struct.unpack('I',value)[0]
                frames[f'{ident:08x}']+=1
                # Classic CAN, intermission included; maximum uses a conservative stuffing bound.
                overhead=67 if ident & EFF else 47
                bits_min+=overhead+8*dlc; bits_max+=overhead+8*dlc+(54+8*dlc if ident & EFF else 34+8*dlc)//4
                event=decoder.feed(ident,data,now)
                if event is None: continue
                if event['kind']=='sync':
                    t1=pending.pop(event['seq'],None)
                    if t1 is not None: clock.update(t1,event)
                elif event['kind']=='status': status=event
                else:
                    mapped=clock.map(event['device_us'],event['boot'],now)
                    event['mapped_mono']=mapped
                    if mapped is not None: ages.append((now-mapped)*1000)
                    if last is not None: intervals.append((now-last)*1000)
                    if first is None: first=now
                    last=now; samples+=1
                    if first_device is None: first_device=event['device_us']
                    last_device=event['device_us']
                    for array,value in zip(axes,event['accel']+event['gyro']): array.append(value)
                log.write(json.dumps(event,separators=(',',':'))+'\n')
        except KeyboardInterrupt: pass
    duration=time.monotonic()-start
    result=dict(duration_s=duration,samples=samples,rate_hz=samples/duration,decoder=dict(decoder.counts),
                silence_ms=dict(initial=(duration if first is None else first-start)*1000,terminal=(duration if last is None else time.monotonic()-last)*1000),
                arrival_ms=dict(p50=percentile(intervals,.5),p99=percentile(intervals,.99),max=max(intervals,default=None)),
                mapped_age_ms=dict(p50=percentile(ages,.5),p99=percentile(ages,.99),max=max(ages,default=None)),
                clock=dict(drift_ppm=(clock.a-1)*1e6,min_rtt_ms=None if clock.min_rtt is None else clock.min_rtt*1000,residual_ms=None if clock.residual is None else clock.residual*1000),
                bus=dict(observe_all=args.observe_bus,sync_requests_sent=sync_sent,frames=sum(frames.values()),by_id=dict(frames),load_lower=bits_min/duration/1e6,load_upper=bits_max/duration/1e6,socket_drops=drops),
                latest_status=status,axis_mean=[statistics.mean(a) if a else None for a in axes],axis_stddev=[statistics.pstdev(a) if a else None for a in axes])
    (args.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2));s.close()

if __name__=='__main__':main()
