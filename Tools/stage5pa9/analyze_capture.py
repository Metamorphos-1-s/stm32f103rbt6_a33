#!/usr/bin/env python3
"""Analyze completed Stage 5P-A9 read-only development capture, no device access."""
import argparse,csv,hashlib,json,statistics
from datetime import datetime
from pathlib import Path
from a9_model_review import FIELDS,direct,edges,sec_series,frozen_r5,oracle,trace_scores

def checksum(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest().upper()

def read(path):
    with path.open(encoding='utf-8',newline='') as f: rows=list(csv.DictReader(f))
    for r in rows:
        r['t']=int(r['host_monotonic_ns'])
        for k in FIELDS:r[k]=int(r[k]) if r.get(k) not in (None,'') else None
    return rows

def edge_bracket(rows, edge, field, next_edge_s=None):
    idx=edge['index'];base=rows[0]['t'];mid=edge['center_s']; before=[];after=[]
    if next_edge_s is not None and next_edge_s<mid+45:return {'status':'NOT AVAILABLE: next edge before full 15–45 s reference'}
    for r in rows[max(0,idx-800):min(len(rows),idx+800)]:
        dt=(r['t']-base)/1e9-mid
        if -30<=dt<-5:before.append(r[field])
        elif 15<=dt<45:after.append(r[field])
    if not before or not after:return {'status':'NOT AVAILABLE'}
    lo=statistics.median(before);hi=statistics.median(after);threshold=(lo+hi)/2
    segment=rows[max(1,idx-400):min(len(rows),idx+400)]
    for j in range(1,len(segment)):
        x,y=segment[j-1][field],segment[j][field]
        if (x<threshold<=y if hi>lo else x>threshold>=y):
            return {'field':field,'before_median':lo,'after_median':hi,
                'bracket_monotonic_ns':[segment[j-1]['t'],segment[j]['t']],
                'bracket_width_s':(segment[j]['t']-segment[j-1]['t'])/1e9,
                'host_utc_labels':[segment[j-1]['utc'],segment[j]['utc']],
                'warning':'asynchronous Modbus registers, not physical ADC DRDY'}
    return {'status':'NOT AVAILABLE','before_median':lo,'after_median':hi}

def quality(rows):
    gaps=duplicates=regressions=uptime_regressions=0;max_gap=0;clock_jumps=[]
    for previous,current in zip(rows,rows[1:]):
        ps,cs=int(previous['sample_sequence']),int(current['sample_sequence'])
        diff=(cs-ps)&0xffffffff
        if diff==0:duplicates+=1
        elif diff<0x80000000:gaps+=max(0,diff-1)
        else:regressions+=1
        m=(current['t']-previous['t'])/1e9;max_gap=max(max_gap,m)
        if int(current['mcu_uptime_ms'])<int(previous['mcu_uptime_ms']):uptime_regressions+=1
        u=(datetime.fromisoformat(current['utc'].replace('Z','+00:00'))-datetime.fromisoformat(previous['utc'].replace('Z','+00:00'))).total_seconds()
        if abs(u-m)>1:clock_jumps.append({'before_utc':previous['utc'],'after_utc':current['utc'],'utc_step_s':u,'monotonic_step_s':m})
    observed=len(rows)-duplicates
    return {'rows':len(rows),'distinct_sample_sequences':observed,'unobserved_sequences':gaps,
            'estimated_sequence_coverage':observed/(observed+gaps) if observed+gaps else None,
            'duplicate_sequences':duplicates,'sequence_regressions':regressions,'uptime_regressions':uptime_regressions,
            'maximum_host_gap_s':max_gap,'utc_clock_jumps':clock_jumps,
            'first_sequence':int(rows[0]['sample_sequence']),'last_sequence':int(rows[-1]['sample_sequence']),
            'first_uptime_ms':int(rows[0]['mcu_uptime_ms']),'last_uptime_ms':int(rows[-1]['mcu_uptime_ms']),
            'faults':sorted(set(int(r['fault_mask']) for r in rows)),
            'save_request_count_low_min_max':[min(int(r['save_request_count_low']) for r in rows),max(int(r['save_request_count_low']) for r in rows)],
            'calibration_valid_values':sorted(set(int(r['calibration_valid']) for r in rows)),
            'official_stable_fraction':sum(int(r['official_stable']) for r in rows)/len(rows),
            'overrun_min_max':[min(int(r['overrun_count']) for r in rows),max(int(r['overrun_count']) for r in rows)],
            'dirty_values':sorted(set(int(r['dirty']) for r in rows)),
            'revision_saved':sorted(set((int(r['revision']),int(r['saved_revision'])) for r in rows)),
            'application_mode':sorted(set((int(r['application']),int(r['mode'])) for r in rows)),
            'offset_min_max_ug':[min(int(r['offset_ug']) for r in rows),max(int(r['offset_ug']) for r in rows)]}

def analyze(directory):
    path=directory/'samples.csv';rows=read(path); events=[json.loads(line) for line in (directory/'events.jsonl').read_text(encoding='utf-8').splitlines() if line]
    ed=edges(rows);phases=direct(rows,ed)
    seconds=sec_series(rows);frozen=frozen_r5(seconds);oracle_trace,oracle_max10,oracle_events=oracle(seconds,ed)
    for i,edge in enumerate(ed):
        next_edge=ed[i+1]['center_s'] if i+1<len(ed) else None
        edge['raw_adc_edge']=edge_bracket(rows,edge,'raw_adc',next_edge)
        edge['filtered_adc_edge']=edge_bracket(rows,edge,'filtered_raw',next_edge)
        edge['gross_edge']=edge_bracket(rows,edge,'gross_ug',next_edge)
        target='LOAD' if edge['kind']=='load' else 'UNLOAD'
        completion=next((x for x in events if x.get('event')==target+'_'+str(sum(z['kind']==edge['kind'] for z in ed[:ed.index(edge)+1]))+'_COMPLETE'),None)
        if completion:
            edge['operator_complete_mark']=completion
            edge['operator_confirmation_lag_s']=(int(completion['host_monotonic_ns'])-int(rows[0]['t']))/1e9-edge['center_s']
    return {'classification':'STAGE5PA9_MODEL_DEVELOPMENT_ATTRIBUTION_NOT_HOLDOUT',
        'input_csv':str(path),'input_sha256':checksum(path),'environment':json.loads((directory/'environment.json').read_text(encoding='utf-8')),
        'first_utc':rows[0]['utc'],'last_utc':rows[-1]['utc'],'monotonic_span_s':(rows[-1]['t']-rows[0]['t'])/1e9,
        'quality':quality(rows),'events':events,'edges':ed,'phases':phases,
        'model_replay':{'classification':'OPENED_A9_DEVELOPMENT_NOT_HOLDOUT',
            'frozen_r5':{'scores':trace_scores(frozen,ed),'automatic_rebuilds':frozen[-1][7],
                'max_10s_offset_ug':max((max(abs(frozen[j][2]-frozen[i][2]) for j in range(i,min(i+11,len(frozen))) if frozen[j][0]-frozen[i][0]<=10) for i in range(len(frozen))),default=0)},
            'oracle_event_feedback':{'scores':trace_scores(oracle_trace,ed),
                'max_10s_offset_ug':oracle_max10,'events':oracle_events,
                'warning':'retrospective foreknowledge; not deployable and not an independent holdout'}},
        'temperature_c':'NOT MEASURED unless manually supplied',
        'uncompensated_identity':'R5 SHADOW/OFF offset=0: authoritative gross_ug equals uncompensated weight; no independent filtered mass register'}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path); ap.add_argument('--output',type=Path);a=ap.parse_args()
    result=analyze(a.directory); target=a.output or a.directory/'attribution_analysis.json';target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps({'rows':result['quality']['rows'],'coverage':result['quality']['estimated_sequence_coverage'],'events':len(result['edges']),'sha256':result['input_sha256']},indent=2))
if __name__=='__main__':main()
