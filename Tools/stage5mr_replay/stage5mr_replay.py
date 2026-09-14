#!/usr/bin/env python3
import argparse,csv,json,statistics,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];CFG={"arm_time_ms":15000,"hold_off_ms":15000,"quiet_range_ug":40000,"step_threshold_ug":20000,"trend_threshold_ug":20000,"update_period_ms":1000,"maximum_update_ug":50,"maximum_offset_ug":500000};CAL=(-43989,-487850,500000000)
def wj(p,x):Path(p).write_bytes((json.dumps(x,indent=2)+"\n").encode())
def d(a,b):(None)
def delta(a,b):return(b-a)&0xffffffff
def conv(raw):return round((raw-CAL[0])*CAL[2]/(CAL[1]-CAL[0]))
class Drift:
 def __init__(s):s.enabled=False;s.offset=0;s.ref=0;s.prev=0;s.h=[];s.state=0;s.reason=0;s.last_t=s.last_seq=s.enter=s.change=s.update=0;s.updates=s.freezes=s.pos=s.neg=0
 def process(s,m,t,q,cal=True,over=False,fault=False,rail=False):
  if not s.enabled:s.enabled=True;s.ref=m;s.prev=m;s.state=1;s.enter=s.change=s.update=t
  if not cal or over or fault or rail:s.state=5;s.reason=7 if fault else 8 if over else 6 if rail else 7
  else:
   reason=0
   if s.last_seq and delta(s.last_seq,q)!=1:reason=4
   elif s.last_t and delta(s.last_t,t)>250:reason=5
   if abs(m-s.prev)>=CFG['step_threshold_ug']:reason=1
   s.h.append(m);s.h=s.h[-16:];rng=max(s.h)-min(s.h)
   if len(s.h)==16 and abs(m-s.h[0])>=CFG['trend_threshold_ug']:reason=3
   if reason:s.state=3;s.reason=reason;s.change=t;s.freezes+=1
   if s.state==3:s.state=4;s.ref=m-s.offset
   elif s.state==4:
    s.ref=m-s.offset
    if delta(s.change,t)>=CFG['hold_off_ms']:s.state=1;s.enter=t;s.h=[]
   elif s.state==1 and len(s.h)==16 and rng<=CFG['quiet_range_ug'] and delta(s.enter,t)>=CFG['arm_time_ms']:s.ref=m-s.offset;s.state=2;s.reason=0;s.update=t
   elif s.state==2 and rng<=CFG['quiet_range_ug'] and delta(s.update,t)>=CFG['update_period_ms']:
    e=m-s.offset-s.ref;u=max(-CFG['maximum_update_ug'],min(CFG['maximum_update_ug'],e));s.offset+=u;s.updates+=1;s.update=t;s.pos+=max(0,u);s.neg+=min(0,u)
  s.prev=m;s.last_t=t;s.last_seq=q;return {"corrected_mass_ug":m-s.offset,"drift_offset_ug":s.offset,"reference_mass_ug":s.ref,"state":s.state,"freeze_reason":s.reason,"update_count":s.updates,"freeze_count":s.freezes,"total_positive_correction_ug":s.pos,"total_negative_correction_ug":s.neg}
def rows(path):
 with (ROOT/path).open(encoding='utf-8',newline='') as f:return list(csv.DictReader(f))
def replay(path):
 a=Drift();out=[]
 for i,r in enumerate(rows(path)):m=int(r['uncompensated_gross_ug']);out.append({**a.process(m,int(r['mcu_uptime_ms']),i+1),"measured_mass_ug":m,"timestamp_ms":int(r['mcu_uptime_ms']),"sample_sequence":i+1})
 return out
def slope(v):
 x=[(z['timestamp_ms']-v[0]['timestamp_ms'])/1000 for z in v];y=[z['measured_mass_ug'] for z in v];z=[q['corrected_mass_ug'] for q in v];xm=statistics.fmean(x)
 def one(a):am=statistics.fmean(a);den=sum((q-xm)**2 for q in x);return sum((q-xm)*(w-am) for q,w in zip(x,a))/den*3600/1e6
 return {"input_g_per_hour":one(y),"corrected_g_per_hour":one(z),"offset_change_g":(v[-1]['drift_offset_ug']-v[0]['drift_offset_ug'])/1e6,"update_count":v[-1]['update_count'],"freeze_count":v[-1]['freeze_count']}
def synthetic(increment_g,interval_s,count=10):
 a=Drift();off=Drift();on=[];baseline=[];m=0;t=0;q=1
 for _ in range(200):baseline.append(m);on.append(a.process(m,t,q));t+=100;q+=1
 for _ in range(count):
  m+=round(increment_g*1e6)
  for _ in range(round(interval_s*10)):baseline.append(m);on.append(a.process(m,t,q));t+=100;q+=1
 loss=(baseline[-1]-baseline[199])-(on[-1]['corrected_mass_ug']-on[199]['corrected_mass_ug'])
 return {"increment_g":increment_g,"interval_s":interval_s,"detected":on[-1]['freeze_count']>0,"loss_g":loss/1e6,"max_offset_g":max(abs(x['drift_offset_ug']) for x in on)/1e6}
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--runner',required=True);a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
 static_paths={"empty1":"Results/stage5lr_hardware/20260914_control1_10hz_cold_60m/samples.csv","empty2":"Results/stage5lr_hardware/20260914_control2_10hz_cold_60m/samples.csv","loaded":"Results/stage5l_characterization/20260913T064617Z_creep_500g_30m/samples.csv"};sm={k:slope(replay(v)) for k,v in static_paths.items()};wj(out/'offline_static_metrics.json',sm)
 scans=[synthetic(i,t) for i in(.001,.002,.005,.01,.02,.05) for t in(.5,1,2,5,10,15)];intervals=(2,5,10);increments=sorted({x['increment_g'] for x in scans});reliable=[i for i in increments if all(x['detected'] for x in scans if x['increment_g']==i and x['interval_s'] in intervals)];protected=[i for i in increments if all(x['loss_g']<=.005 for x in scans if x['increment_g']==i and x['interval_s'] in intervals)];wj(out/'minimum_protected_increment.json',{"scan":scans,"minimum_observed_real_increment_g":0.001127,"minimum_reliably_detected_g":min(reliable,default=None),"minimum_protected_increment_g":min(protected,default=None),"protected_definition":"final loss <=0.005 g for each 2/5/10 s pause; detection not implied","ten_second_worst_theoretical_g":CFG['maximum_update_ug']*10/1e6,"fifteen_second_worst_theoretical_g":CFG['maximum_update_ug']*15/1e6})
 slow=replay('Results/stage5l_characterization/20260913_slow_fill/faster_continuous/samples.csv');wj(out/'offline_drip_metrics.json',{"continuous_change":slope(slow),"offset_change_g":(slow[-1]['drift_offset_ug']-slow[0]['drift_offset_ug'])/1e6,"freeze_count":slow[-1]['freeze_count'],"synthetic_scans":len(scans)})
 load=[]
 for n in ('filt1_load_step','filt1_unload_step'):
  v=replay(f'Results/stage5l_characterization/20260913_filter_compare/{n}/samples.csv');load.append({"run":n,"offset_change_g":(v[-1]['drift_offset_ug']-v[0]['drift_offset_ug'])/1e6,"freeze_count":v[-1]['freeze_count'],"final_on_minus_off_g":-(v[-1]['drift_offset_ug'])/1e6})
 wj(out/'offline_load_protection.json',{"runs":load})
 # Python/C equivalence on actual records
 src=rows(static_paths['empty2'])[:1000];lines=[];ref=[];model=Drift()
 for i,r in enumerate(src):m=int(r['uncompensated_gross_ug']);t=int(r['mcu_uptime_ms']);lines.append(f'{t},{i+1},{m},1,0,0,0');ref.append(model.process(m,t,i+1))
 got=list(csv.DictReader(subprocess.run([a.runner],input='\n'.join(lines)+'\n',text=True,stdout=subprocess.PIPE,check=True).stdout.splitlines()));fields=list(ref[0]);bad=sum(any(int(x[f])!=y[f] for f in fields) for x,y in zip(got,ref));wj(out/'fixed_point_equivalence.json',{"samples":len(ref),"mismatch_count":bad,"result":"PASS" if bad==0 else "FAIL"});wj(out/'algorithm_config.json',CFG)
 no_reverse=all(abs(x['corrected_g_per_hour'])<=abs(x['input_g_per_hour']) for x in sm.values());ok=bad==0 and no_reverse and all(abs(x['final_on_minus_off_g'])<=.005 for x in load) and min(protected,default=99)<=.005;wj(out/'selection.json',{"offline_safe":ok,"minimum_protected_increment_g":min(protected,default=None),"minimum_reliably_detected_g":min(reliable,default=None),"static_no_reverse_amplification":no_reverse,"failed_run":"empty1" if not no_reverse else None,"hardware_shadow_authorized":ok});print('OFFLINE','PASS' if ok else 'FAIL')
if __name__=='__main__':main()
