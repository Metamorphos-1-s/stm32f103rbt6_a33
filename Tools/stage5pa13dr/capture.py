"""One serial owner; cumulative target evidence survives skipped host polls."""
import argparse
import csv
import hashlib
import json
import sys
import time
from pathlib import Path
from contract import Checker,decode,PATHS
from restore import restore
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/stage5pa13c'))
from hardware_shadow import SerialTransport,ModbusClient,now,unsigned32,word32,word64
from hw_common import execute_command

def read(client):
    rt,_=client.read(0,32); d,_=client.read(0x20,28)
    storage,_=client.read(0x1c0,10); beta,_=client.read(0x280,40)
    words,_=client.read(0x380,122)
    target=decode(words)
    safety=dict(firmware=rt[15],map=rt[14],dirty=d[18],revision=unsigned32(d[19:21],'high'),
        saved_revision=unsigned32(d[21:23],'high'),save_count=beta[39],application=beta[1],calibration_valid=d[27])
    original=dict(gross_ug=word64(rt[20:24],'high'),raw_adc=word32(rt[28:30],'high'),
        filtered_adc_counts=word32(rt[30:32],'high'),display_count=word32(rt[:2],'high'),
        beta_input_ug=word64(beta[10:14],'high'),beta_corrected_ug=word64(beta[14:18],'high'),
        beta_offset_ug=word64(beta[6:10],'high'),beta_mode=beta[2],beta_state=beta[3],beta_limited=beta[4],
        note='earlier_read_blocks_not_same_sample_as_cumulative_snapshot')
    return target,safety,original

def run(args):
    out=args.output;out.mkdir(parents=True,exist_ok=True)
    if (out/'polls.csv').exists():raise ValueError('raw record already exists')
    events=(out/'events.jsonl').open('a',encoding='utf-8')
    frames=(out/'snapshots.jsonl').open('w',encoding='utf-8')
    def event(kind,**data):
        events.write(json.dumps(dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),kind=kind,**data))+'\n');events.flush()
    checker=Checker();polls=0;seen=0;mode=0;failure=None;latest=None
    deadline=time.monotonic()+args.duration;last_update=0
    try:
        with SerialTransport('COM5',115200,'N',1,350) as transport:
            client=ModbusClient(transport,1)
            with (out/'polls.csv').open('w',newline='',encoding='utf-8') as stream:
                writer=None
                event('CAPTURE_BEGIN',contract='TARGET_CUMULATIVE',five_blocks=[[0,32],[32,28],[448,10],[640,40],[896,122]])
                while time.monotonic()<deadline:
                    requests=out/'requests.jsonl'
                    lines=requests.read_text().splitlines() if requests.exists() else []
                    for line in lines[seen:]:
                        request=json.loads(line);event('HOST_REQUEST',request=request)
                        if request.get('mode') is not None:
                            result=execute_command(client,1500+seen,29,arg0=int(request['mode']))
                            event('MODE_RESULT',result=result)
                            if result['result']:raise ValueError('mode rejected')
                            mode=int(request['mode'])
                        if request.get('stop'):deadline=0
                        seen+=1
                    if not deadline:break
                    target,safety,original=read(client)
                    stamp=time.monotonic_ns()
                    record=dict(utc=now(),host_monotonic_ns=stamp,target=target,safety=safety,original=original)
                    frames.write(json.dumps(record)+'\n');frames.flush()
                    flat={k:v for k,v in target.items() if k not in ('maxima','counts','peaks')}
                    flat.update(safety);flat.update(original);flat['utc']=record['utc'];flat['host_monotonic_ns']=stamp
                    for i,path in enumerate(PATHS):
                        flat[path+'_max_cycles']=target['maxima'][i]
                        flat[path+'_count']=target['counts'][i]
                        flat[path+'_peak_sequence']=target['peaks'][i]
                    if writer is None:writer=csv.DictWriter(stream,fieldnames=list(flat));writer.writeheader()
                    writer.writerow(flat);stream.flush();polls+=1;latest=record
                    checker.check(target,stamp,safety)
                    if target['mode']!=mode or original['beta_limited']:raise ValueError('unexpected mode/LIMITED')
                    if time.monotonic()-last_update>=1:
                        (out/'live_summary.json').write_text(json.dumps(dict(status='RUNNING',polls=polls,
                            observed_host_gap_events=len(checker.gaps),qualified=checker.qualified,
                            requests_consumed=seen,latest=record),indent=2)+'\n')
                        last_update=time.monotonic()
    except Exception as exc:
        failure=str(exc);event('EVIDENCE_FAILURE',error=failure)
    finally:
        event('CAPTURE_END',failure=failure)
        frames.close()
        if latest:(out/'final_target_snapshot.json').write_text(json.dumps(latest,indent=2)+'\n')
        try:
            restore(out/'restore');event('EXACT_051D_RESTORED')
        except Exception as exc:
            failure=(failure or '')+'; restore: '+str(exc);event('RESTORE_BLOCKED',error=str(exc))
        result=dict(status='FAIL' if failure else checker.complete(),failure=failure,polls=polls,
            host_gap_events=checker.gaps,
            polls_sha256=hashlib.sha256((out/'polls.csv').read_bytes()).hexdigest().upper(),
            snapshots_sha256=hashlib.sha256((out/'snapshots.jsonl').read_bytes()).hexdigest().upper())
        (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
        (out/'live_summary.json').write_text(json.dumps(result,indent=2)+'\n')
        events.close();print(json.dumps(result),flush=True)

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--duration',type=float,default=3500);p.add_argument('--event')
    p.add_argument('--mode',type=int,choices=(0,1,2));p.add_argument('--stop',action='store_true')
    a=p.parse_args()
    if a.event or a.mode is not None or a.stop:
        a.output.mkdir(parents=True,exist_ok=True)
        with (a.output/'requests.jsonl').open('a') as stream:
            stream.write(json.dumps(dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),event=a.event,mode=a.mode,stop=a.stop))+'\n')
    else:run(a)
if __name__=='__main__':main()
