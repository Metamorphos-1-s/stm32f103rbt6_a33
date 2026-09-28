"""One COM5 owner; pre-frozen MCU cumulative resource capture and restoration."""
import argparse
import csv
import hashlib
import json
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/stage5pa13dr'))
sys.path.insert(0,str(ROOT/'Tools/stage5pa13c'))
sys.path.insert(0,str(ROOT/'Tools/stage5b_hw'))
from capture import read
from hardware_shadow import SerialTransport,ModbusClient,now,unsigned32
from hw_common import execute_command
from resource_contract import Checker,PATHS
from hardware import restore


def append(path,obj):
    with path.open('a',encoding='utf-8') as file:file.write(json.dumps(obj)+'\n')


def run(args):
    out=args.output;out.mkdir(parents=True,exist_ok=True)
    if (out/'snapshots.jsonl').exists():raise ValueError('raw evidence already exists')
    checker=Checker();seen=0;polls=0;error=None;last=None;mode=0;application=0;transition_deadline=0
    deadline=time.monotonic()+args.duration;next_summary=0
    stamp=lambda kind,**data:append(out/'events.jsonl',dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),kind=kind,**data))
    try:
        with SerialTransport('COM5',115200,'N',1,350) as port:
            client=ModbusClient(port,1)
            with (out/'snapshots.jsonl').open('w',encoding='utf-8') as frames, (out/'polls.csv').open('w',newline='',encoding='utf-8') as csvfile:
                writer=None;stamp('CAPTURE_START',identity='0x0521/0x010A',five_blocks=[[0,32],[32,28],[448,10],[640,40],[896,122]])
                while time.monotonic()<deadline:
                    q=out/'requests.jsonl';requests=q.read_text().splitlines() if q.exists() else []
                    for line in requests[seen:]:
                        req=json.loads(line);stamp('HOST_REQUEST',request=req)
                        if req.get('mode') is not None:
                            generation,_=client.read(0x0342,2)
                            gen=unsigned32(generation,'high')
                            response=execute_command(client,1600+seen,36,arg0=req['application'],arg1=req['mode'],arg64=gen,flags=1)
                            stamp('PAIR_RESULT',result=response,expected_generation=gen)
                            if response['result']:raise ValueError('atomic pair rejected: '+str(response))
                            mode=req['mode'];application=req['application']
                            transition_deadline=time.monotonic()+1
                        if req.get('stop'):deadline=0
                        seen+=1
                    if not deadline:break
                    target,safety,original=read(client);host=time.monotonic_ns()
                    frame=dict(utc=now(),host_monotonic_ns=host,target=target,safety=safety,original=original)
                    frames.write(json.dumps(frame)+'\n');frames.flush()
                    flat=dict(utc=frame['utc'],host_monotonic_ns=host,firmware=safety['firmware'],
                        map=safety['map'],application=safety['application'],mode=target['mode'],
                        sample_sequence=target['last'],mcu_ms=target['now_ms'],
                        gross_ug=original['gross_ug'],display_count=original['display_count'],
                        candidate_offset_ug=original['beta_offset_ug'],
                        cumulative_peak_cycles=target['max_cycles'],loop_peak_cycles=target['loop_max'],
                        stack_untouched_bytes=target['touched']-target['static_end'],
                        coverage=target['coverage'],read_errors=target['read_errors'],
                        overrun=target['overrun'],fault=target['fault'])
                    for i,path in enumerate(PATHS):
                        flat[path+'_count']=target['counts'][i];flat[path+'_max_cycles']=target['maxima'][i]
                    if writer is None:writer=csv.DictWriter(csvfile,fieldnames=list(flat));writer.writeheader()
                    writer.writerow(flat);csvfile.flush();polls+=1;last=frame
                    checker.check(target,host,safety)
                    if original['beta_limited'] or ((target['mode']!=mode or
                        safety['application']!=application) and time.monotonic()>transition_deadline):
                        raise ValueError('unexpected LIMITED/mode')
                    if time.monotonic()>next_summary:
                        (out/'live_summary.json').write_text(json.dumps(dict(status='RUNNING',polls=polls,
                            host_gaps=len(checker.gaps),last=frame,requests_consumed=seen),indent=2)+'\n')
                        next_summary=time.monotonic()+1
    except Exception as exc:
        error=str(exc);stamp('CAPTURE_FAILURE',error=error)
    finally:
        stamp('CAPTURE_END',error=error)
        if last:(out/'final_target_snapshot.json').write_text(json.dumps(last,indent=2)+'\n')
        try:
            restored=restore(out/'restore')
            stamp('EXACT_051D_RESTORED',terminal=restored['firmware'])
        except Exception as exc:
            error=(error or '')+'; restore: '+str(exc)
            stamp('RESTORE_FAILURE',error=str(exc))
        result=dict(status='FAIL' if error else checker.complete(),error=error,polls=polls,
            host_gap_events=checker.gaps,
            snapshots_sha256=hashlib.sha256((out/'snapshots.jsonl').read_bytes()).hexdigest().upper()
                if (out/'snapshots.jsonl').exists() else None,
            polls_sha256=hashlib.sha256((out/'polls.csv').read_bytes()).hexdigest().upper()
                if (out/'polls.csv').exists() else None,
            note='target cumulative maxima survive host gaps; earlier weight read blocks are not same sample')
        (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
        (out/'live_summary.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result),flush=True)


def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--duration',type=float,default=1800);p.add_argument('--event')
    p.add_argument('--application',type=int,choices=(0,1))
    p.add_argument('--mode',type=int,choices=(0,1,2));p.add_argument('--stop',action='store_true')
    args=p.parse_args()
    if args.mode is not None and args.application is None:p.error('--mode requires --application')
    if args.event or args.mode is not None or args.stop:
        args.output.mkdir(parents=True,exist_ok=True)
        append(args.output/'requests.jsonl',dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),
            event=args.event,mode=args.mode,application=args.application,stop=args.stop))
    else:run(args)
if __name__=='__main__':main()
