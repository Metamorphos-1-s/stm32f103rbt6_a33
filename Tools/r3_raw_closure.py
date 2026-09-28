"""Focused R3 raw evidence, no flashing/configuration SAVE/calibration writes."""
import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools/stage5b_hw'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from hw_common import ModbusClient, execute_command
from serial_transport import SerialTransport
from hardware_shadow import probe, now

def u32(w, i): return (w[i] << 16) | w[i+1]
def i32(v): return v if v < 0x80000000 else v - 0x100000000
def i64(w, i):
    v = (u32(w, i) << 32) | u32(w, i+2)
    return v if v < (1 << 63) else v - (1 << 64)

def decode(w):
    if len(w) != 40: raise ValueError('A13E atomic block length')
    return dict(signature=u32(w,0), generation=u32(w,2),
        exit_offset_ug=i32(u32(w,4)), exit_reason=u32(w,6), application=u32(w,8),
        sequence=u32(w,10), mcu_ms=u32(w,12), applied_offset_ug=i32(u32(w,14)),
        apply=u32(w,16), uncompensated_gross_ug=i64(w,18), gross_ug=i64(w,22),
        net_ug=i64(w,26), tare_ug=i64(w,30), mode=u32(w,34), state=u32(w,36),
        candidate_offset_ug=i32(u32(w,38)))

class LoggedClient(ModbusClient):
    def __init__(self, transport, file):
        super().__init__(transport, 1); self.file=file
    def raw(self, request, expect_response=True):
        before=time.monotonic_ns()
        try:
            exchange=super().raw(request,expect_response)
        except Exception as error:
            self.file.write(json.dumps(dict(utc=now(),host_monotonic_ns=before,
                tx_hex=bytes(request).hex(),error=str(error)))+'\n'); self.file.flush()
            raise
        self.file.write(json.dumps(dict(utc=now(),host_start_ns=before,
            host_end_ns=time.monotonic_ns(),tx_hex=exchange.tx.hex(),rx_hex=exchange.rx.hex()))+'\n')
        self.file.flush(); return exchange

def atomic(client):
    w,_=client.read(0x340,40)
    result=decode(w)
    expected=result['uncompensated_gross_ug']-(result['applied_offset_ug'] if result['apply'] else 0)
    if result['signature']!=0xA13E0001 or result['gross_ug']!=expected or result['net_ug']!=result['gross_ug']-result['tare_ug']:
        raise ValueError('atomic authority arithmetic failure')
    return result

def snapshot(client):
    before=atomic(client)
    diag,_=client.read(0x20,28)
    panel,_=client.read(0,32)
    beta,_=client.read(0x280,40)
    candidate,_=client.read(0x300,37)
    cw,_=client.read(0x2c0,8)
    after=atomic(client)
    coherent=(before['sequence']==after['sequence']==u32(diag,0)==u32(candidate,17)
        and before['generation']==after['generation'])
    return dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),atomic=after,
        bracket_before=before,multi_block_same_sample=coherent,firmware=panel[15],map=panel[14],
        calibration_valid=diag[27],fault=u32(diag,25),overrun=u32(diag,13),
        dirty=diag[18],revision=u32(diag,19),saved_revision=u32(diag,21),
        sample_sequence=u32(diag,0),mcu_uptime_ms=u32(diag,2),
        display_count=i32(u32(panel,0)),display_decimals=panel[2],
        status_flags=panel[4]|(panel[5]<<16),raw_adc=i32(u32(panel,28)),
        candidate_sequence=u32(candidate,17),candidate_offset_ug=i64(candidate,5),
        candidate_corrected_ug=i64(candidate,9),candidate_uncompensated_ug=i64(candidate,13),
        candidate_limited=candidate[4],beta_words=beta,save_count=beta[39],checkweigh_words=cw)

def healthy(s, allow_dirty=False):
    if s['firmware']!=0x0520 or s['map']!=0x0109 or s['fault'] or s['overrun'] or s['save_count'] or not s['calibration_valid'] or s['candidate_limited']:
        raise ValueError('identity/safety failure')
    if not allow_dirty and (s['dirty'] or s['revision']!=19 or s['saved_revision']!=19):
        raise ValueError('unexpected persistence change')

def run(out, duration):
    if out.exists(): raise ValueError('existing evidence directory; cannot overwrite')
    out.mkdir(parents=True)
    with (out/'frames.jsonl').open('x') as frames, (out/'samples.jsonl').open('x') as samples, (out/'events.jsonl').open('x') as events:
        def save(kind, **data):
            events.write(json.dumps(dict(utc=now(),host_monotonic_ns=time.monotonic_ns(),kind=kind,**data))+'\n'); events.flush()
        with SerialTransport('COM5',115200,'N',1,350) as transport:
            c=LoggedClient(transport,frames)
            def command(kind,code,arg0=0,arg1=0,arg64=0,flags=0):
                previous,_=c.read(0x4c,12)
                token=(previous[0]+1)&65535
                if not token: token=1
                pre=snapshot(c)
                save('COMMAND_REQUEST',name=kind,token=token,code=code,arg0=arg0,arg1=arg1,arg64=arg64,flags=flags,previous_response_words=previous,pre=pre)
                result=execute_command(c,token,code,arg0,arg1,arg64,flags)
                response,_=c.read(0x4c,12)
                post=snapshot(c)
                save('COMMAND_RESULT',name=kind,result=result,response_words=response,post=post)
                return result,post
            def pair(kind,app,mode):
                gen=atomic(c)['generation']
                r,p=command(kind,36,app,mode,gen,1)
                if r['result'] or p['atomic']['application']!=app or p['atomic']['mode']!=mode:
                    raise ValueError('pair command/postcondition failure')
                return p
            def capture(seconds):
                end=time.monotonic()+seconds; count=0; nonzero=0; matched=0; offsets=set()
                while time.monotonic()<end:
                    s=snapshot(c); samples.write(json.dumps(s)+'\n');samples.flush(); healthy(s)
                    a=s['atomic']; count+=1; nonzero+=a['candidate_offset_ug']!=0; offsets.add(a['candidate_offset_ug'])
                    matched+=s['multi_block_same_sample']
                    if s['multi_block_same_sample'] and (a['candidate_offset_ug']!=s['candidate_offset_ug'] or a['uncompensated_gross_ug']!=s['candidate_uncompensated_ug'] or a['gross_ug']!=s['candidate_corrected_ug'] and a['application']==1):
                        raise ValueError('candidate versus authority relation failed')
                return dict(polls=count,nonzero=nonzero,matched=matched,offsets=sorted(offsets))
            error=None
            try:
                initial=probe(c);save('PREFLIGHT',probe=initial)
                healthy(snapshot(c))
                if initial['r5']['application'] or initial['r5']['mode'] or initial['r5']['offset_ug'] or initial['checkweigh']['mode']:
                    raise ValueError('unexpected volatile initial state')
                save('H1_OFF',capture=capture(2))
                pair('H1_ZERO_ACTIVE',1,2)
                save('H1_ACTIVE_STATIC',capture=capture(duration))
                pair('H1_DOSING',1,1)
                freeze=capture(6);save('H1_DOSING',capture=freeze)
                if len(freeze['offsets'])!=1:raise ValueError('DOSING offset changed')
                for mode in (1,2):
                    r,p=command('H3_ACTIVE_REJECT_CHECKWEIGH_'+str(mode),34,mode)
                    healthy(p)
                    if r['result']!=4 or p['checkweigh_words'][1] or p['atomic']['application']!=1:
                        raise ValueError('ACTIVE/checkweigh exclusion failure')
                pair('H3_OFF',0,0)
                r,p=command('H3_CHECKWEIGH_ON',34,1)
                if r['result'] or p['checkweigh_words'][1]!=1:raise ValueError('checkweigh enable failed')
                # Explicitly prove all admission conditions except Checkweigh before rejecting ACTIVE.
                pre=probe(c);a=atomic(c);safe=snapshot(c)
                eligible=(pre['calibration_valid'] and not pre['fault_mask'] and not pre['overrun_count']
                    and pre['configuration']['sample_rate']==0 and pre['configuration']['filter_mode']==1
                    and pre['configuration']['filter_strength']==3 and a['mode']==0
                    and a['candidate_offset_ug']==0 and not safe['candidate_limited']
                    and safe['status_flags']&8 and not safe['status_flags']&128
                    and abs(safe['raw_adc'])<8323072)
                save('H3_REVERSE_PRECONDITIONS',eligible_except_checkweigh=bool(eligible),probe=pre,atomic=a,snapshot=safe)
                if not eligible:raise ValueError('cannot exclude alternative admission reasons')
                r,p=command('H3_CHECKWEIGH_REJECT_ACTIVE',36,1,2,a['generation'],1)
                if r['result']!=4 or p['atomic']['application'] or p['atomic']['mode'] or p['checkweigh_words'][1]!=1:
                    raise ValueError('reverse exclusion failure')
            except Exception as exc:
                error=str(exc);save('FAILURE',error=error)
            finally:
                try:
                    if atomic(c)['application'] or atomic(c)['mode']:pair('CLEANUP_OFF',0,0)
                    r,p=command('CLEANUP_CHECKWEIGH_OFF',34,0)
                    if r['result'] or p['checkweigh_words'][1]:raise ValueError('cleanup Checkweigh failure')
                    save('TERMINAL',probe=probe(c),snapshot=snapshot(c))
                except Exception as exc:
                    error=(error or '')+'; cleanup: '+str(exc);save('CLEANUP_FAILURE',error=str(exc))
            (out/'summary.json').write_text(json.dumps(dict(error=error,status='FAIL' if error else 'EVIDENCE_COLLECTED_PENDING_OFFLINE_REVIEW'),indent=2)+'\n')
            print(json.dumps(dict(error=error,out=str(out))),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--duration',type=float,default=100)
    args=p.parse_args();run(args.out,args.duration)
