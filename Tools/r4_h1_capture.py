"""Only H1: raw-first bounded natural-offset capture, explicit safe cleanup."""
import argparse
import json
import sys
import time
from pathlib import Path
from r3_raw_closure import decode,u32,i64
from hw_common import ModbusClient,execute_command
from serial_transport import SerialTransport
from hardware_shadow import probe,now
from modbus_frame import verify_crc,registers_from_read_response

ROOT=Path(__file__).resolve().parents[1]

def unique_directory(path):
    path.mkdir(parents=True,exist_ok=False)
    return path

def next_token(previous):
    return ((previous+1)&65535) or 1

def validate_atomic(a):
    if a['signature']!=0xA13E0001:raise ValueError('atomic signature changed')
    if abs(a['candidate_offset_ug'])>500000 or abs(a['applied_offset_ug'])>500000:
        raise ValueError('offset outside engineering envelope')
    gross_error=a['gross_ug']-(a['uncompensated_gross_ug']-(a['applied_offset_ug'] if a['apply'] else 0))
    net_error=a['net_ug']-(a['gross_ug']-a['tare_ug'])
    if gross_error or net_error:raise ValueError('atomic weight equation mismatch')
    if a['application']==1 and (not a['apply'] or a['mode']==0 or a['applied_offset_ug']!=a['candidate_offset_ug']):
        raise ValueError('ACTIVE application/output inconsistency')
    if a['application']==0 and a['apply']:raise ValueError('SHADOW applied candidate')
    return gross_error,net_error

def parse_fc03(tx,rx):
    if len(tx)!=8 or tx[0]!=1 or tx[1]!=3 or not verify_crc(tx) or not verify_crc(rx):
        raise ValueError('invalid raw FC03/CRC')
    values=registers_from_read_response(rx,1)
    address=int.from_bytes(tx[2:4],'big');quantity=int.from_bytes(tx[4:6],'big')
    if len(values)!=quantity:raise ValueError('raw response quantity mismatch')
    parsed=dict(address=address,quantity=quantity,words=values)
    if address==0x340 and quantity==40:parsed['atomic']=decode(values)
    return parsed

class JournalClient(ModbusClient):
    def __init__(self,transport,file):
        super().__init__(transport,1);self.file=file;self.frame_id=0
    def raw(self,request,expect_response=True):
        self.frame_id+=1
        record=dict(frame_id=self.frame_id,utc=now(),host_start_ns=time.monotonic_ns(),tx_hex=bytes(request).hex())
        try:
            exchange=super().raw(request,expect_response)
            record.update(host_end_ns=time.monotonic_ns(),rx_hex=exchange.rx.hex())
            if request[1]==3:record['parsed']=parse_fc03(exchange.tx,exchange.rx)
            elif not verify_crc(exchange.tx) or not verify_crc(exchange.rx):raise ValueError('write response CRC')
            record['valid']=True
        except Exception as error:
            record.update(valid=False,error=str(error),host_end_ns=time.monotonic_ns())
            raise
        finally:
            self.file.write(json.dumps(record)+'\n');self.file.flush()
        return exchange

def atomic(client):
    words,_=client.read(0x340,40)
    a=decode(words)
    record=dict(frame_id=client.frame_id,utc=now(),host_monotonic_ns=time.monotonic_ns(),atomic=a)
    # Caller archives record before checking numerical conditions.
    return record

def preconditions(device):
    config=device['configuration'];r5=device['r5']
    if (device['firmware'],device['map'])!=('0x0520','0x0109'):
        raise ValueError('unexpected firmware/map; no mode write permitted')
    if (config['sample_rate'],config['filter_mode'],config['filter_strength'])!=(0,1,3):
        raise ValueError('unexpected active profile; no configuration changes permitted')
    if (device['revision'],device['saved_revision'])!=(19,19) or any(device[k] for k in ('dirty','fault_mask','overrun_count')):
        raise ValueError('unexpected revision/dirty/safety state')
    if not device['calibration_valid'] or config['persistent_format']!=3:
        raise ValueError('invalid calibration/persistent format')
    if (config['raw_zero'],config['raw_span'],config['span_mass_ug'])!=(41868,485780,500000000):
        raise ValueError('unexplained calibration endpoint change')
    if any(r5[k] for k in ('application','mode','offset_ug','limited','save_request_count_low')) or device['checkweigh']['mode']:
        raise ValueError('unexpected volatile state; no activation permitted')

class Consecutive:
    def __init__(self):self.last=None;self.count=0;self.gaps=[];self.duplicates=0
    def feed(self,sequence,eligible):
        if self.last is not None:
            if sequence<self.last:raise ValueError('MCU sample sequence regressed')
            if sequence==self.last:
                self.duplicates+=1;return self.count
            if sequence>self.last+1:
                self.gaps.append(dict(previous=self.last,current=sequence,missing=sequence-self.last-1))
                self.count=0
        self.last=sequence
        self.count=self.count+1 if eligible else 0
        return self.count

def capture(out,wait_s=1200,static_samples=20,dosing_samples=100):
    if not 0<wait_s<=1200:raise ValueError('natural wait must be bounded to 1200s')
    unique_directory(out)
    error=None;incomplete=None;activated=False;cleaned=False;pre=None;terminal=None
    started=time.monotonic_ns();static_count=0;dos_count=0;read_errors=0
    with (out/'frames.jsonl').open('x',encoding='utf-8') as frames, (out/'samples.jsonl').open('x',encoding='utf-8') as samples, (out/'events.jsonl').open('x',encoding='utf-8') as events:
        def event(kind,**data):
            events.write(json.dumps(dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),kind=kind,**data))+'\n');events.flush()
        def record(client,phase):
            r=atomic(client);r['phase']=phase;samples.write(json.dumps(r)+'\n');samples.flush()
            validate_atomic(r['atomic']);return r
        def progress(phase,last=None):
            (out/'live_summary.json').write_text(json.dumps(dict(status='RUNNING',phase=phase,
                last=last,elapsed_s=(time.monotonic_ns()-started)/1e9,
                static_nonzero_consecutive=static_count,dosing_nonzero_consecutive=dos_count),indent=2)+'\n')
        try:
            with SerialTransport('COM5',115200,'N',1,350) as transport:
                c=JournalClient(transport,frames)
                def pair(name,app,mode):
                    before=record(c,'command_pre_'+name)
                    previous,_=c.read(0x4c,12);token=next_token(previous[0]);gen=before['atomic']['generation']
                    event('COMMAND_REQUEST',name=name,token=token,code=36,arg0=app,arg1=mode,arg64=gen,flags=1,
                        previous_response_words=previous,pre=before)
                    response=execute_command(c,token,36,app,mode,gen,1)
                    full,_=c.read(0x4c,12)
                    after=record(c,'command_post_'+name)
                    event('COMMAND_RESULT',name=name,result=response,response_words=full,post=after)
                    a=after['atomic']
                    if response['result'] or full[0]!=token or full[3]!=36 or a['generation']!=gen+1 or (a['application'],a['mode'])!=(app,mode):
                        raise ValueError('pair response/generation/state mismatch; no retry')
                    return after
                def safety():
                    d=probe(c);event('SAFETY',probe=d)
                    if (d['firmware'],d['map'])!=('0x0520','0x0109') or any(d[k] for k in ('dirty','fault_mask','overrun_count')) or d['revision']!=19 or d['saved_revision']!=19 or d['r5']['limited'] or d['r5']['save_request_count_low'] or d['checkweigh']['mode']:
                        raise ValueError('identity/safety/persistence changed')
                    return d
                try:
                    # Raw file is already open before the first preflight query.
                    pre=probe(c);event('PREFLIGHT',probe=pre);preconditions(pre)
                    first=record(c,'OFF_BASELINE')
                    if any(first['atomic'][k] for k in ('application','mode','candidate_offset_ug','applied_offset_ug','apply')):
                        raise ValueError('atomic OFF precondition mismatch')
                    event('RAW_RECORDER_VALIDATED',frame_id=first['frame_id'],offset=0)
                    activated=True  # cleanup also covers a command whose response is lost
                    pair('ZERO_OFFSET_ACTIVE_STATIC',1,2)
                    static_tracker=Consecutive();deadline=time.monotonic()+wait_s;next_safety=0;next_live=0;last_ms=None
                    while time.monotonic()<deadline:
                        r=record(c,'ACTIVE_STATIC');a=r['atomic']
                        if (a['application'],a['mode'],a['apply'])!=(1,2,1) or a['state']==8:
                            raise ValueError('unexpected STATIC application/mode/LIMITED')
                        if last_ms is not None and a['mcu_ms']<last_ms:raise ValueError('MCU time regressed')
                        last_ms=a['mcu_ms']
                        static_count=static_tracker.feed(a['sequence'],a['applied_offset_ug']!=0)
                        if time.monotonic()>=next_safety:safety();next_safety=time.monotonic()+5
                        if time.monotonic()>=next_live:progress('WAIT_NATURAL_NONZERO',r);next_live=time.monotonic()+1
                        if static_count>=static_samples:break
                        time.sleep(0.035)
                    event('STATIC_SUMMARY',consecutive_nonzero=static_count,gaps=static_tracker.gaps,duplicates=static_tracker.duplicates)
                    if static_count<static_samples:
                        incomplete='no sufficient natural nonzero STATIC window within 1200s'
                    else:
                        switch=pair('ACTIVE_DOSING_PRESERVE_OFFSET',1,1)
                        frozen=switch['atomic']['applied_offset_ug']
                        if frozen==0:incomplete='offset naturally reached zero at DOSING transition'
                        else:
                            dosing_tracker=Consecutive();deadline=time.monotonic()+60;next_safety=0
                            while time.monotonic()<deadline:
                                r=record(c,'ACTIVE_DOSING');a=r['atomic']
                                if (a['application'],a['mode'],a['state'],a['apply'])!=(1,1,1,1):raise ValueError('unexpected DOSING state')
                                if a['applied_offset_ug']!=frozen or a['candidate_offset_ug']!=frozen:
                                    raise ValueError('DOSING offset not strictly frozen')
                                dos_count=dosing_tracker.feed(a['sequence'],True)
                                if time.monotonic()>=next_safety:safety();next_safety=time.monotonic()+15
                                progress('NONZERO_DOSING',r)
                                if dos_count>=dosing_samples:break
                                time.sleep(0.035)
                            event('DOSING_SUMMARY',offset_ug=frozen,consecutive_samples=dos_count,gaps=dosing_tracker.gaps,duplicates=dosing_tracker.duplicates)
                            if dos_count<dosing_samples:incomplete='insufficient consecutive nonzero DOSING samples within60s'
                except (Exception,KeyboardInterrupt) as exc:
                    error=str(exc) or type(exc).__name__;event('CAPTURE_ERROR',error=error)
                finally:
                    if activated:
                        try:
                            pair('CLEANUP_OFF_SHADOW',0,0)
                            terminal=probe(c);reference,_=c.read(0x292,4);off=record(c,'OFF_TERMINAL')
                            ref=i64(reference,0)
                            event('TERMINAL',probe=terminal,reference_ug=ref,atomic=off)
                            preconditions(terminal)
                            if ref or any(off['atomic'][k] for k in ('application','mode','apply','applied_offset_ug','candidate_offset_ug')):
                                raise ValueError('terminal not clean OFF+SHADOW')
                            cleaned=True
                        except Exception as exc:
                            error=(error or '')+'; cleanup: '+str(exc);event('CLEANUP_ERROR',error=str(exc))
        except (Exception,KeyboardInterrupt) as exc:
            error=(error or '')+'; transport: '+(str(exc) or type(exc).__name__)
            read_errors+=1;event('TRANSPORT_ERROR',error=error)
        result=dict(status='FAIL' if error else 'INCOMPLETE' if incomplete or not cleaned else 'CAPTURE_COMPLETE_PENDING_RAW_REVIEW',
            error=error,incomplete=incomplete,activated=activated,clean_terminal=cleaned,
            static_nonzero_consecutive=static_count,dosing_nonzero_consecutive=dos_count,
            preflight=pre,terminal=terminal,transport_errors=read_errors,elapsed_s=(time.monotonic_ns()-started)/1e9)
        event('CAPTURE_END',result=result)
        (out/'capture_summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        (out/'live_summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({k:result[k] for k in ('status','error','incomplete','clean_terminal','static_nonzero_consecutive','dosing_nonzero_consecutive','elapsed_s')}),flush=True)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--wait',type=float,default=1200)
    a=p.parse_args();r=capture(a.out,a.wait)
    raise SystemExit(1 if r['status']=='FAIL' else 0)
