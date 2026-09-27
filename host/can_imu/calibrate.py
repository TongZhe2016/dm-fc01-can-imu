#!/usr/bin/env python3
"""Generate reviewable USB calibration commands from stationary CAN recordings.

For gyro: --gyro samples.jsonl. For accel: --faces faces.json, containing
{x+: file, x-: file, y+: file, y-: file, z+: file, z-: file} with device axes.
Record uncalibrated data (bias=0, scale=1, temperature slope=0) for the selected sensor.
"""
import argparse
import json
import math
from pathlib import Path
import statistics


def stationary(path,source):
    rows=[json.loads(s) for s in Path(path).read_text().splitlines() if s.strip()]
    rows=[r for r in rows if r.get('kind')=='sample']
    if len(rows)<1000:raise ValueError('At least five seconds / 1000 samples required')
    identities={(r['boot'],r['epoch'],r['source']) for r in rows}
    if len(identities)!=1 or rows[0]['source']!=source:raise ValueError('Mixed session/config/source')
    if any(not r['flags']&1 or r['flags']&16 for r in rows):raise ValueError('Invalid/clipped samples')
    if any(b['device_us']-a['device_us']!=5000 for a,b in zip(rows,rows[1:])):raise ValueError('Sampling gaps')
    axes=list(zip(*(r['accel']+r['gyro'] for r in rows)))
    means=[statistics.mean(v) for v in axes]; std=[statistics.pstdev(v) for v in axes]
    if max(std[:3])>.08 or max(std[3:])>.01 or math.sqrt(sum(v*v for v in means[3:]))>.1:raise ValueError('Motion/noise exceeds stationary limit')
    if not 8.5<math.sqrt(sum(v*v for v in means[:3]))<11:raise ValueError('Gravity magnitude outside calibration range')
    return means,std,rows[0]['epoch']


def main():
    p=argparse.ArgumentParser(description=__doc__);m=p.add_mutually_exclusive_group(required=True)
    m.add_argument('--gyro',type=Path);m.add_argument('--faces',type=Path)
    p.add_argument('--source',type=int,choices=[0,1],default=0)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    commands=[];evidence={}
    if a.gyro:
        mean,std,epoch=stationary(a.gyro,a.source)
        for axis,value in zip('XYZ',mean[3:]):commands.append(f'param set CI{a.source}_G{axis}B {value:.9g}')
        commands.append(f'param set CI{a.source}_GCAL 1');evidence=dict(mean=mean,std=std)
    else:
        faces=json.loads(a.faces.read_text());values={};epochs=set()
        for face in ('x+','x-','y+','y-','z+','z-'):
            path=Path(faces[face]);path=path if path.is_absolute() else a.faces.parent/path
            mean,std,epoch=stationary(path,a.source);values[face]=mean;epochs.add(epoch)
            axis='xyz'.index(face[0]);sign=1 if face[1]=='+' else -1
            if mean[axis]*sign<9 or max(abs(mean[k]) for k in range(3) if k!=axis)>1:raise ValueError(f'{face}: not aligned with labelled device axis')
        if len(epochs)!=1:raise ValueError('All faces must share one calibration epoch')
        for i,axis in enumerate('XYZ'):
            hi,lo=values[axis.lower()+'+'][i],values[axis.lower()+'-'][i]
            bias=(hi+lo)/2;scale=2*9.80665/(hi-lo)
            if not .9<scale<1.1 or abs(bias)>.5:raise ValueError('Implausible calibration')
            commands.extend([f'param set CI{a.source}_A{axis}B {bias:.9g}',f'param set CI{a.source}_A{axis}S {scale:.9g}'])
        commands.append(f'param set CI{a.source}_ACAL 1');evidence=values
    commands.extend([f'param set CI_EPOCH {(epoch+1)%65536}','param save','reboot'])
    a.output.write_text(json.dumps(dict(source=a.source,commands=commands,evidence=evidence),indent=2)+'\n')
    print('\n'.join(commands))

if __name__=='__main__':main()
