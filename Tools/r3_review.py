"""Offline R3 evidence review; no serial/SWD calls, no failed-record rewrites."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
from r3_raw_closure import decode
from modbus_frame import registers_from_read_response,verify_crc

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Results/stage5pa13er3'

def load_json(path):
    data=path.read_bytes()
    encoding='utf-16' if data.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'
    return json.loads(data.decode(encoding))
def lines(path):
    return [json.loads(t) for t in path.read_text(encoding='utf-8-sig').splitlines() if t.strip()]
def parsed_stats(path):
    records=lines(path)
    pairs=list(zip(records,records[1:]))
    deltas=[b['sequence']-a['sequence'] for a,b in pairs]
    return dict(rows=len(records),first=records[0]['sequence'] if records else None,
        last=records[-1]['sequence'] if records else None,
        gross_mismatches=sum(r['formal_gross']!=r['uncompensated_gross']-(r['applied_offset'] if r['apply'] else 0) for r in records),
        net_mismatches=sum(r['formal_net']!=r['formal_gross']-r['tare'] for r in records),
        duplicates=sum(d==0 for d in deltas),forward_gap_events=sum(d>1 for d in deltas),
        unobserved_sequences=sum(max(d-1,0) for d in deltas),sequence_backwards=sum(d<0 for d in deltas),
        dos_samples=sum(r['mode']==1 for r in records),
        dosing_offsets=sorted({r['offset'] for r in records if r['mode']==1}),
        classification='PARSED_ONLY; missing original RTU responses')

def raw_review(path):
    frames=lines(path);counts=collections.Counter();decoded=[];errors=[]
    for n,frame in enumerate(frames):
        if frame.get('error'):
            errors.append(dict(frame=n,error=frame['error']));continue
        tx=bytes.fromhex(frame['tx_hex']);rx=bytes.fromhex(frame['rx_hex'])
        if not verify_crc(tx) or not verify_crc(rx):raise ValueError('RTU CRC failure frame '+str(n))
        counts[tx[1]]+=1
        if tx[1]==3:
            words=registers_from_read_response(rx,1)
            address=int.from_bytes(tx[2:4],'big');quantity=int.from_bytes(tx[4:6],'big')
            if len(words)!=quantity:raise ValueError('FC03 wrong quantity')
            if address==0x340 and quantity==40:
                a=decode(words)
                if a['gross_ug']!=a['uncompensated_gross_ug']-(a['applied_offset_ug'] if a['apply'] else 0) or a['net_ug']!=a['gross_ug']-a['tare_ug']:
                    raise ValueError('raw frame arithmetic failure')
                decoded.append(a)
    return dict(frames=len(frames),function_counts=dict(counts),read_errors=errors,
        raw_atomic_reads=len(decoded),raw_nonzero_offset_reads=sum(a['candidate_offset_ug']!=0 for a in decoded),
        gross_mismatches=0,net_mismatches=0)

def review():
    raw=BASE/'raw_recovery';samples=lines(raw/'samples.jsonl');events=lines(raw/'events.jsonl')
    requests={e['name']:e for e in events if e['kind']=='COMMAND_REQUEST'}
    results={e['name']:e for e in events if e['kind']=='COMMAND_RESULT'}
    for name,r in requests.items():
        response=results[name]
        if r['token']==r['previous_response_words'][0]:raise ValueError('stale reused mailbox token')
        if response['response_words'][0]!=r['token'] or response['result']['last_command']!=r['code']:
            raise ValueError('request/response identity mismatch')
    eligible=next(e for e in events if e['kind']=='H3_REVERSE_PRECONDITIONS')
    h3_pass=bool(eligible['eligible_except_checkweigh'])
    for name in ('H3_ACTIVE_REJECT_CHECKWEIGH_1','H3_ACTIVE_REJECT_CHECKWEIGH_2'):
        pre=requests[name]['pre'];post=results[name]['post'];response=results[name]['result']
        h3_pass=h3_pass and response['result']==4 and pre['atomic']['application']==post['atomic']['application']==1
        h3_pass=h3_pass and pre['atomic']['generation']==post['atomic']['generation']
        h3_pass=h3_pass and pre['checkweigh_words']==post['checkweigh_words']
        h3_pass=h3_pass and (pre['dirty'],pre['revision'],pre['saved_revision'])==(post['dirty'],post['revision'],post['saved_revision'])
    reverse='H3_CHECKWEIGH_REJECT_ACTIVE';pre=requests[reverse]['pre'];post=results[reverse]['post']
    h3_pass=h3_pass and results[reverse]['result']['result']==4
    h3_pass=h3_pass and pre['atomic']['application']==post['atomic']['application']==0
    h3_pass=h3_pass and pre['atomic']['candidate_offset_ug']==post['atomic']['candidate_offset_ug']==0
    h3_pass=h3_pass and pre['atomic']['generation']==post['atomic']['generation']==requests[reverse]['arg64']
    h3_pass=h3_pass and pre['checkweigh_words']==post['checkweigh_words']
    h3_pass=h3_pass and (pre['dirty'],pre['revision'],pre['saved_revision'])==(post['dirty'],post['revision'],post['saved_revision'])
    terminal=load_json(BASE/'terminal_after_cleanup_power/terminal.json')
    p=terminal['device'];a=terminal['snapshot']['atomic']
    safe=(p['firmware']=='0x0520' and p['map']=='0x0109' and p['revision']==p['saved_revision']==19
        and not any(p[k] for k in ('dirty','fault_mask','overrun_count')) and not p['checkweigh']['mode']
        and not any(a[k] for k in ('application','mode','apply','applied_offset_ug','candidate_offset_ug'))
        and terminal['reference_ug']==0 and terminal['snapshot']['save_count']==0)
    frame_info=raw_review(raw/'frames.jsonl')
    terminal_frames=raw_review(BASE/'terminal_after_cleanup_power/frames.jsonl')
    if frame_info['read_errors'] or terminal_frames['read_errors'] or not safe:raise ValueError('raw/final safety failure')
    coherent=[s for s in samples if s['multi_block_same_sample']]
    for s in coherent:
        a=s['atomic'];beta=s['beta_words']
        # All three candidate weight surfaces must have matching sequence brackets.
        from r3_raw_closure import i64
        if (a['uncompensated_gross_ug']!=s['candidate_uncompensated_ug'] or
            a['candidate_offset_ug']!=s['candidate_offset_ug'] or
            a['candidate_offset_ug']!=i64(beta,6) or
            a['uncompensated_gross_ug']!=i64(beta,10) or
            a['gross_ug']!=s['candidate_corrected_ug'] and a['application']==1):
            raise ValueError('coherent R5/candidate relation mismatch')
    return dict(result='INCOMPLETE: H1 nonzero raw-response gate remains open',
        H1='INCOMPLETE',H2='PASS default-OFF behavior after user-confirmed physical power-cycle; initial 30s not observed',
        H3='PASS' if h3_pass else 'INCONCLUSIVE',
        h1_parsed_nonzero=parsed_stats(BASE/'h1_final/samples.jsonl'),
        h1_raw=frame_info,h1_raw_polls=len(samples),h1_raw_cross_block_same_sequence=len(coherent),
        h1_raw_nonzero_polls=sum(s['atomic']['candidate_offset_ug']!=0 for s in samples),
        h3_preconditions_eligible=eligible['eligible_except_checkweigh'],
        terminal_safe=safe,terminal_utc=p['utc'],terminal_uptime_ms=p['mcu_uptime_ms'],
        terminal=terminal,terminal_raw_frames=terminal_frames,
        missing=['original responses for prior nonzero-offset STATIC/DOSING arithmetic',
            'nonzero offset in raw-complete recovery capture',
            'first 30 seconds/reference immediately after H2 power-on (not recoverable)',
            'human lamp/buzzer observations: NOT RUN'],
        preserved_history=['R2 resource 511/511 PASS unchanged','A13B efficacy INCONCLUSIVE unchanged',
            'historical A13D FAIL unchanged'],
        firmware_source_changed=False,no_flash=True,no_save=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();result=review()
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('result','H1','H2','H3','terminal_safe','h1_raw_polls','h1_raw_cross_block_same_sequence','h1_raw_nonzero_polls')}))
