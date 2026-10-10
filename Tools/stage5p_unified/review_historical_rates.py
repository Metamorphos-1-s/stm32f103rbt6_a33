"""Read archived profile CSVs as historical context, never A13C qualification."""
import csv
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Results/stage5pa1b/hardware/20260924T_profile_switch_closure'

def review(path,expected_rate):
    csv_path=path/'samples.csv'
    info=json.loads((path/'summary.json').read_text(encoding='utf-8-sig'))
    sequences=[];stamps=[];identity=set();rates=set();faults=[];overruns=[]
    with csv_path.open(encoding='utf-8-sig',newline='') as stream:
        for row in csv.DictReader(stream):
            sequences.append(int(row['sample_sequence']))
            stamps.append(int(row['uptime_ms']))
            identity.add((row['firmware'],row['map']))
            rates.add(int(row['sample_rate']))
            faults.append(int(row['fault_mask']))
            overruns.append(int(row['overrun_count']))
    if not sequences:raise ValueError('empty history')
    gaps=[b-a-1 for a,b in zip(sequences,sequences[1:])]
    first=info['first_state'];last=info['last_state']
    if rates!={expected_rate} or first['sample_sequence']!=sequences[0] or last['sample_sequence']!=sequences[-1]:
        raise ValueError('rate or source-summary mismatch')
    return dict(source=csv_path.relative_to(ROOT).as_posix(),
        sha256=hashlib.sha256(csv_path.read_bytes()).hexdigest().upper(),
        records=len(sequences),sequence_span=sequences[-1]-sequences[0]+1,
        observed_fraction=len(set(sequences))/(sequences[-1]-sequences[0]+1),
        missing_sequences=sum(max(0,g) for g in gaps),host_duplicate_polls=sum(g<0 for g in gaps),
        timestamp_span_ms=stamps[-1]-stamps[0],recorded_firmware_map=sorted(identity),
        sample_rate=expected_rate,fault_nonzero=sum(v!=0 for v in faults),
        overrun_growth=overruns[-1]-overruns[0],summary_status=info['status'],
        filter_label=path.name.split('_')[2],
        filter_strength='NOT RECORDED IN THIS CSV; folder label is not a synchronized register read',
        epoch='2026-09-24, before corrected S+/S- wiring and before A13C 0x0523')

def main():
    output=[]
    for rate in (10,40):
        for mode in range(4):
            name=f'h5_{rate}_filt{mode}_30s'
            output.append(review(BASE/name,0 if rate==10 else 1))
    result=dict(classification='HISTORICAL_SPARSE_HOST_POLLS_NOT_UNIFIED_A13C_QUALIFICATION',
        rows=output,limitations=['older 0x0519 R5 algorithm, not unified A13C 0x0523',
            'collected before S+/S- correction',
            'missing physical 40 Hz samples cannot be interpolated for state-machine replay',
            'filter strength not present in synchronized CSV fields'])
    out=ROOT/'Results/stage5p_unified/software/historical_rate_review.json'
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps([(r['source'].split('/')[-2],r['records'],r['missing_sequences'],
        round(r['observed_fraction'],3)) for r in output]))

if __name__=='__main__':main()
