#!/usr/bin/env python3
"""Stage 5P-A8 read-only review of the opened post-wiring CSV."""
import argparse,csv,hashlib,json,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
DEFAULT_INPUT=ROOT/'Results/stage5pa4/realtime_runs/20260925T121451Z_0x051C/samples.csv'
EXPECTED_SHA='10545968A92A23688162A68D82DCFCF20C10FDD0ABDF3EC58203F6E549D0C3D'
FIELDS=('gross_ug','raw_adc','filtered_raw','display_count','conditioned_display_ug','display_anchor_ug')
def sha256(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
 return h.hexdigest().upper()
def read_rows(p):
 with p.open(encoding='utf-8',newline='') as f:r=list(csv.DictReader(f))
 for x in r:
  x['t']=int(x['host_monotonic_ns'])
  for k in FIELDS:x[k]=int(x[k]) if x.get(k) not in (None,'') else None
 return r
def detect_edges(r):
 out=[]; loaded=r[0]['gross_ug']>=250_000_000
 for i in range(1,len(r)):
  now=r[i]['gross_ug']>=250_000_000
  if now==loaded:continue
  a,b=r[i-1],r[i]; br=[(a['t']-r[0]['t'])/1e9,(b['t']-r[0]['t'])/1e9]
  out.append({'kind':'load' if now else 'unload','index':i,'bracket_s':br,'width_s':br[1]-br[0],'center_s':sum(br)/2,'before_g':a['gross_ug']/1e6,'after_g':b['gross_ug']/1e6});loaded=now
 return out
def phases(r,e):
 total=(r[-1]['t']-r[0]['t'])/1e9;out=[]
 for i,x in enumerate(e):
  end=e[i+1]['center_s'] if i+1<len(e) else total;out.append({'edge':x,'duration_s':end-x['center_s']})
 return out
def series(r):
 base=r[0]['t'];bs={}
 for x in r:bs.setdefault(int((x['t']-base)/1e9),[]).append(x['gross_ug'])
 return [(s,int(statistics.median(v))) for s,v in sorted(bs.items())]
def candidate(s,e,rate,favorable):
 es=[round(x['center_s']) for x in e]; masses=dict(s); off=0; ref=None; until=-1; t=[]
 edge_ref={round(x['center_s']): statistics.median([m for z,m in s if round(x['center_s'])+15<=z<round(x['center_s'])+45]) for x in e}
 for sec,m in s:
  hit=next((z for z in es if abs(sec-z)<=2),None)
  if favorable and hit is not None: until=sec+60; ref=None
  if favorable and sec<until: mode='DOSING'
  else:
   mode='STATIC'
   if favorable and ref is None:
    edge=min(es,key=lambda z:abs(z-sec))
    if abs(edge-sec)<=60 and edge in edge_ref: ref=int(edge_ref[edge])
   if ref is None: ref=m if ref is None else ref
   off+=max(-rate,min(rate,m-ref-off))
  t.append((sec,m-off,off,mode))
 mx=0
 for i,(sec,_,o,_) in enumerate(t):
  q=[x[2] for x in t[i:i+11]];mx=max(mx,max((abs(v-o) for v in q),default=0))
 return t,mx
def score(t,e):
 out=[]
 for x in e:
  z=round(x['center_s']);a=[v for s,v,_,_ in t if z+15<=s<z+45];b=[v for s,v,_,_ in t if z+1740<=s<z+1800]
  out.append({'kind':x['kind'],'early_corrected_g':statistics.median(a)/1e6 if a else None,'30m_corrected_g':statistics.median(b)/1e6 if b else None})
 return out
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,default=DEFAULT_INPUT);ap.add_argument('--output',type=Path,default=ROOT/'Results/stage5pa8/review.json');a=ap.parse_args();d=sha256(a.input);r=read_rows(a.input);e=detect_edges(r);p=phases(r,e);s=series(r);cs={}
 for n,rate,fav in [('favorable_event_feedback_25ug_s',25,True),('favorable_event_feedback_50ug_s',50,True),('all_static_feedback_50ug_s',50,False)]:
  t,m=candidate(s,e,rate,fav);cs[n]={'rate_ug_s':rate,'favorable_schedule':fav,'max_10s_offset_change_ug':m,'scores':score(t,e),'final_offset_ug':t[-1][2]}
 o={'classification':'STAGE5PA8_OPENED_DATA_REVIEW_NOT_HOLDOUT','input':str(a.input),'sha256':d,'sha_expected':EXPECTED_SHA,'sha_match':d==EXPECTED_SHA,'records':len(r),'duration_s':(r[-1]['t']-r[0]['t'])/1e9,'fields':list(r[0].keys()),'event_method':'gross_ug crossing 250 g; host_monotonic_ns brackets; no human markers','edges':e,'phases':p,'longest_loaded_phase_s':max((x['duration_s'] for x in p if x['edge']['kind']=='load'),default=0),'candidates':cs,'decision':'MODEL SELECTION INCOMPLETE','reason':'opened data has no independent event markers, temperature control, or holdout; candidate behavior is schedule-sensitive'};a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(o,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps({'sha256':d,'records':len(r),'edges':e,'longest_loaded_phase_s':o['longest_loaded_phase_s'],'decision':o['decision']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()





