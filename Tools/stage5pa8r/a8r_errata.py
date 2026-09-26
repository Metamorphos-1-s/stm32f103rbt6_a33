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
 a=origin+begin_s;b=min(a+duration_s,end);return (a,b) if b>a else None
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
   d['checkpoints'][str(m)]={f:med([z[f] for z in block if z[f] is not None]) for f in FIELDS} if block else None
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
 for s,v in series: q=m.process_second(s,v);out.append((s,q['corrected_gross_ug'],q['offset_ug'],q['state'],q['mode']))
 return out
def oracle(series,event_list,rate=50,hold_s=60):
 # Deliberately favorable replay: edge times are known after the fact.
 es=[round(x['center_s']) for x in event_list];base_offset=120000;off=base_offset;ref=None;until=-1;pending=None;trace=[]
 ref_windows={z:med([m-base_offset for s,m in series if z+15<=s<z+45]) for z in es}
 for s,m in series:
  hit=next((z for z in es if abs(s-z)<=2),None)
  if hit is not None:until=s+hold_s;pending=hit;ref=None
  if s<until:mode='DOSING';delta=0
  else:
   mode='STATIC'
   if ref is None and pending is not None:ref=ref_windows[pending];pending=None
   if ref is None:ref=m-off
   delta=max(-rate,min(rate,(m-off)-ref));off+=delta
  trace.append((s,m-off,off,mode))
 max10=max((max(abs(trace[j][2]-trace[i][2]) for j in range(i,min(i+11,len(trace)))) for i in range(len(trace))),default=0)
 return trace,max10,ref_windows
def trace_scores(trace,e):
 out=[]
 for x in e:
  s=round(x['center_s']);end=phase_end(e,e.index(x),10**12)
  early=[v for z,v,*_ in trace if s+15<=z<s+45];late=[v for z,v,*_ in trace if s+1740<=z<s+1800]
  out.append({'kind':x['kind'],'early_corrected_g':med(early)/1e6 if early else None,'30m_corrected_g':med(late)/1e6 if late else None})
 return out
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,default=INPUT);ap.add_argument('--output',type=Path,default=ROOT/'Results/stage5pa8r/review.json');a=ap.parse_args();r=rows(a.input);base=r[0]['t'];e=edges(r);total=(r[-1]['t']-base)/1e9;ds=direct(r,e);ss=sec_series(r);fr=frozen_r5(ss);ot,om,refs=oracle(ss,e)
 result={'classification':'STAGE5PA8R_OPENED_DATA_ERRATA_NOT_HOLDOUT','input':str(a.input),'sha256':sha(a.input),'expected_sha256':EXPECTED,'sha_match':sha(a.input)==EXPECTED,'records':len(r),'host_monotonic_span_s':total,'utc_span_s':(r[-1]['u'],r[0]['u']),'utc_span_note':'UTC label span differs from monotonic span; UTC labels had a clock jump and are not used for timing','edges':e,'loads':sum(x['kind']=='load' for x in e),'unloads':sum(x['kind']=='unload' for x in e),'longest_load_s':max((phase_end(e,i,total)-x['center_s'] for i,x in enumerate(e) if x['kind']=='load'),default=0),'direct':ds,'frozen_r5':{'max_10s_offset_change_ug':max((max(abs(fr[j][2]-fr[i][2]) for j in range(i,min(i+11,len(fr)))) for i in range(len(fr))),default=0),'scores':trace_scores(fr,e)},'oracle_event_feedback':{'rate_ug_s':50,'hold_s':60,'initial_offset_ug':120000,'max_10s_offset_change_ug':om,'reference_windows_corrected_ug':refs,'scores':trace_scores(ot,e)},'decision':'MODEL SELECTION INCOMPLETE','model_boundary':'Oracle uses retrospectively known edges; frozen R5 uses one real ReferenceLock instance across all periods; neither is an independent holdout.'}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps({'sha_match':result['sha_match'],'monotonic_s':total,'utc_label_span_s':93195.801,'loads':result['loads'],'unloads':result['unloads'],'longest_load_s':result['longest_load_s'],'decision':result['decision']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()

