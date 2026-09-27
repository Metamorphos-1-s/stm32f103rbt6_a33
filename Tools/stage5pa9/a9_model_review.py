#!/usr/bin/env python3
"""Stage 5P-A8R offline errata; never accesses hardware."""
import argparse,csv,hashlib,json,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
INPUT=ROOT/'Results/stage5pa4/realtime_runs/20260925T121451Z_0x051C/samples.csv'
EXPECTED='10545968A92A23688162A68D82DCFCF20C10FDD0ABDF3EC58203F6E549D0C3D2'
FIELDS=('gross_ug','raw_adc','filtered_raw','display_count','conditioned_display_ug','display_anchor_ug')

def sha(p):
 h=hashlib.sha256();
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest().upper()
def rows(p):
 with p.open(encoding='utf-8',newline='') as f:r=list(csv.DictReader(f))
 for x in r:
  x['t']=int(x['host_monotonic_ns']);x['u']=x['utc']
  for k in FIELDS:x[k]=int(x[k]) if x.get(k) not in ('',None) else None
 return r
def med(v):return statistics.median(v) if v else None
def edges(r):
 out=[];loaded=r[0]['gross_ug']>=250_000_000
 for i in range(1,len(r)):
  now=r[i]['gross_ug']>=250_000_000
  if now!=loaded:
   a,b=r[i-1],r[i];out.append({'kind':'load' if now else 'unload','index':i,'lo_s':(a['t']-r[0]['t'])/1e9,'hi_s':(b['t']-r[0]['t'])/1e9,'center_s':(a['t']+b['t']-2*r[0]['t'])/2e9,'width_s':(b['t']-a['t'])/1e9,'before_g':a['gross_ug']/1e6,'after_g':b['gross_ug']/1e6});loaded=now
 return out
def phase_end(e,i,total):return e[i+1]['center_s'] if i+1<len(e) else total
def clipped_window(origin,end,begin_s,duration_s):
 a=origin+begin_s;b=a+duration_s;return (a,b) if b<=end else None
def direct(r,e):
 base=r[0]['t'];total=(r[-1]['t']-base)/1e9;out=[]
 for i,x in enumerate(e):
  start=x['center_s'];end=phase_end(e,i,total)
  def q(a,b):return [z for z in r if a<= (z['t']-base)/1e9 < min(b,end)]
  pre=q(max(0,start-300),start);early=q(start+15,start+45);end5=q(max(start,end-300),end)
  d={'event':x,'phase_duration_s':end-start,'pre5':{},'early':{},'phase_end5':{},'checkpoints':{}}
  for k in ('pre5','early','phase_end5'):
   block={'pre5':pre,'early':early,'phase_end5':end5}[k];d[k]={f:med([z[f] for z in block if z[f] is not None]) for f in FIELDS}
  d['span_early_g']=(d['early']['gross_ug']-d['pre5']['gross_ug'])/1e6 if d['early']['gross_ug'] is not None and d['pre5']['gross_ug'] is not None else None
  d['span_end_g']=(d['phase_end5']['gross_ug']-d['pre5']['gross_ug'])/1e6 if d['phase_end5']['gross_ug'] is not None and d['pre5']['gross_ug'] is not None else None
  for m in (1,2,5,10,15,30,45):
   w=clipped_window(start,end,m*60,60)
   block=[] if w is None else [z for z in r if w[0]<= (z['t']-base)/1e9 < w[1]]
   if block and (block[0]['t']-base)/1e9 <= w[0]+5 and (block[-1]['t']-base)/1e9 >= w[1]-5:
    sample={f:med([z[f] for z in block if z[f] is not None]) for f in FIELDS}
    sample['samples']=len(block)
    sample['delta_from_early']={f:sample[f]-d['early'][f] if sample[f] is not None and d['early'][f] is not None else None for f in FIELDS}
    sample['display_minus_gross_g']=sample['display_count']/100-sample['gross_ug']/1e6 if sample['display_count'] is not None and sample['gross_ug'] is not None else None
    d['checkpoints'][str(m)]=sample
   else:
    d['checkpoints'][str(m)]={'status':'NOT AVAILABLE'}
  for label in ('early','phase_end5'):
   sample=d[label]
   sample['display_minus_gross_g']=sample['display_count']/100-sample['gross_ug']/1e6 if sample['display_count'] is not None and sample['gross_ug'] is not None else None
  out.append(d)
 return out
def sec_series(r):
 base=r[0]['t'];b={}
 for x in r:b.setdefault(int((x['t']-base)/1e9),[]).append(x['gross_ug'])
 return [(s,int(med(v))) for s,v in sorted(b.items())]
def frozen_r5(series):
 import sys;sys.path.insert(0,str(ROOT/'Tools/stage5mr5b_beta'))
 from reference_lock_model import ReferenceLock,Mode
 m=ReferenceLock();m.set_mode(Mode.STATIC_COMPENSATION);out=[]
 for s,v in series: q=m.process_second(s,v);out.append((s,q['corrected_gross_ug'],q['offset_ug'],q['state'],q['mode'],q['reference_ug'],q['last_rebase_reason'],q['automatic_rebase_count']))
 return out
def oracle(series,event_list,rate=50,hold_s=60,initial_offset=0):
    """Oracle replay with per-event inherited offset; research-only."""
    events=sorted(round(x['center_s']) for x in event_list)
    off=initial_offset; ref=None; until=-1; pending=None; next_event=0; trace=[]; event_records=[]
    for sec,mass in series:
        if next_event < len(events) and abs(sec-events[next_event]) <= 2:
            edge=events[next_event]; event_offset=off; ref_window=[m-event_offset for s,m in series if edge+15<=s<edge+45]
            pending={'edge_s':edge,'offset_at_edge_ug':event_offset,'locked_reference_ug':None,
                     'reference_window_ug':med(ref_window),'reason':'EVENT_DOSING'}
            until=edge+hold_s; ref=None; next_event += 1
        if sec < until:
            mode='DOSING'; reason='DOSING_FREEZE'; delta=0
        else:
            mode='STATIC'; reason=''
            if ref is None and pending is not None:
                ref=pending['reference_window_ug']; pending['locked_reference_ug']=ref
                event_records.append(dict(pending)); pending=None; reason='REFERENCE_LOCK'
            if ref is None:
                ref=mass-off; reason='INITIAL_REFERENCE'
            delta=max(-rate,min(rate,(mass-off)-ref)); off=max(-500_000,min(500_000,off+delta))
        trace.append((sec,mass-off,off,mode,ref,reason))
    max10=max((max(abs(trace[j][2]-trace[i][2]) for j in range(i,min(i+11,len(trace))) if trace[j][0]-trace[i][0]<=10) for i in range(len(trace))),default=0)
    return trace,max10,event_records
def trace_scores(trace, events):
    """Phase-relative medians; complete 60-second windows only, never cross an edge."""
    out=[]; total=trace[-1][0]+1
    for i,event in enumerate(events):
        start=event['center_s'];end=phase_end(events,i,total)
        def block(a,b):
            return [row for row in trace if a<=row[0]<b] if b<=end else []
        early=block(start+15,start+45)
        pre=[row for row in trace if max(0,start-300)<=row[0]<start]
        tail=block(max(start,end-300),end)
        eb=med([r[1] for r in early]);pb=med([r[1] for r in pre]);tb=med([r[1] for r in tail])
        checkpoints={}
        for minute in ((1,2,5,10,30) if event['kind']=='load' else (1,2,5,15,30,45)):
            subset=block(start+minute*60,start+(minute+1)*60)
            center=med([r[1] for r in subset]);offset=med([r[2] for r in subset])
            checkpoints[str(minute)]={'corrected_g':center/1e6,'change_from_early_g':(center-eb)/1e6,'offset_ug':offset} if center is not None and eb is not None else {'status':'NOT AVAILABLE'}
        out.append({'kind':event['kind'],'duration_s':end-start,
                    'early_corrected_g':eb/1e6 if eb is not None else None,
                    'pre5_corrected_g':pb/1e6 if pb is not None else None,
                    'phase_end5_corrected_g':tb/1e6 if tb is not None else None,
                    'span_early_g':(eb-pb)/1e6 if eb is not None and pb is not None else None,
                    'span_end_g':(tb-pb)/1e6 if tb is not None and pb is not None else None,
                    'checkpoints':checkpoints})
    return out
def write_trajectory(path, series, frozen, oracle_trace):
    assert len(series)==len(frozen)==len(oracle_trace)
    with path.open('w',encoding='utf-8',newline='') as stream:
        writer=csv.writer(stream,lineterminator='\n')
        writer.writerow(('relative_monotonic_s','measured_gross_ug','frozen_corrected_ug',
                         'frozen_offset_ug','frozen_mode','frozen_state','frozen_reference_ug',
                         'frozen_rebuild_reason','frozen_rebuild_count','oracle_corrected_ug',
                         'oracle_offset_ug','oracle_mode','oracle_locked_reference_ug','oracle_reason'))
        for input_row,baseline,oracle_row in zip(series,frozen,oracle_trace):
            assert input_row[0]==baseline[0]==oracle_row[0]
            writer.writerow((input_row[0],input_row[1],baseline[1],baseline[2],baseline[4],baseline[3],baseline[5],baseline[6],baseline[7],oracle_row[1],oracle_row[2],oracle_row[3],oracle_row[4],oracle_row[5]))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,default=INPUT);ap.add_argument('--output',type=Path,default=ROOT/'Results/stage5pa9/opened_model_review.json');a=ap.parse_args();r=rows(a.input);base=r[0]['t'];e=edges(r);total=(r[-1]['t']-base)/1e9;ds=direct(r,e);ss=sec_series(r);fr=frozen_r5(ss);ot,om,events_oracle=oracle(ss,e)
 result={'classification':'STAGE5PA9_OPENED_DATA_MODEL_DEVELOPMENT_NOT_HOLDOUT','input':str(a.input),'sha256':sha(a.input),'expected_sha256':EXPECTED,'sha_match':sha(a.input)==EXPECTED,'records':len(r),'host_monotonic_span_s':total,'utc_span_s':(r[-1]['u'],r[0]['u']),'utc_span_note':'UTC label span differs from monotonic span; UTC labels had a clock jump and are not used for timing','edges':e,'loads':sum(x['kind']=='load' for x in e),'unloads':sum(x['kind']=='unload' for x in e),'longest_load_s':max((phase_end(e,i,total)-x['center_s'] for i,x in enumerate(e) if x['kind']=='load'),default=0),'direct':ds,'frozen_r5':{'max_10s_offset_change_ug':max((max(abs(fr[j][2]-fr[i][2]) for j in range(i,min(i+11,len(fr))) if fr[j][0]-fr[i][0]<=10) for i in range(len(fr))),default=0),'scores':trace_scores(fr,e),'automatic_rebuild_count':fr[-1][7], 'final_offset_ug':fr[-1][2]},'oracle_event_feedback':{'rate_ug_s':50,'hold_s':60,'initial_offset_ug':0,'max_10s_offset_change_ug':om,'events':events_oracle,'scores':trace_scores(ot,e),'final_offset_ug':ot[-1][2]},'decision':'MODEL SELECTION INCOMPLETE','model_boundary':'Oracle uses retrospectively known edges; frozen R5 uses one real ReferenceLock instance across all periods; neither is an independent holdout.'}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');write_trajectory(a.output.parent/'opened_replay_trajectory.csv',ss,fr,ot);print(json.dumps({'sha_match':result['sha_match'],'monotonic_s':total,'utc_label_span_s':93195.801,'loads':result['loads'],'unloads':result['unloads'],'longest_load_s':result['longest_load_s'],'decision':result['decision']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()









