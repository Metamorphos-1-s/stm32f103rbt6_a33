import csv,json,statistics
from pathlib import Path
p=Path('Results/stage5pa4/realtime_runs/20260925T121451Z_0x051C/samples.csv'); r=list(csv.DictReader(p.open()))
for x in r: x['t']=int(x['host_monotonic_ns']); x['g']=int(x['gross_ug']); x['raw']=int(x['raw_adc']); x['filt']=int(x['filtered_raw']); x['disp']=int(x['display_count'])
base=r[0]['t']; rel=lambda x:(x['t']-base)/1e9
e=[]; loaded=r[0]['g']>=250000000
for i in range(1,len(r)):
 n=r[i]['g']>=250000000
 if n!=loaded: e.append({'kind':'load' if n else 'unload','center':(rel(r[i-1])+rel(r[i]))/2}); loaded=n
def med(a,k): return statistics.median([x[k] for x in a]) if a else None
def win(a,b): return [x for x in r if a<=rel(x)<b]
out=[]
for i,x in enumerate(e):
 prev=e[i-1]['center'] if i else None; nxt=e[i+1]['center'] if i+1<len(e) else rel(r[-1])
 pre=win(x['center']-300,x['center']); early=win(x['center']+15,x['center']+45); end=win(nxt-300,nxt) if nxt-x['center']>300 else []
 out.append({'event':x,'pre5':{k:med(pre,k) for k in ['g','raw','filt','disp']},'early':{k:med(early,k) for k in ['g','raw','filt','disp']},'phase_end5':{k:med(end,k) for k in ['g','raw','filt','disp']},'span_early_g':(med(early,'g')-med(pre,'g'))/1e6 if pre and early else None,'span_end_g':(med(end,'g')-med(pre,'g'))/1e6 if pre and end else None})
# checkpoints relative each edge
for z in out:
 z['checkpoints']={}
 for m in (1,2,5,10,15,30,45):
  q=win(z['event']['center']+m*60,z['event']['center']+m*60+60); z['checkpoints'][str(m)]={k:med(q,k) for k in ['g','raw','filt','disp']} if q else None
Path('Results/stage5pa8/direct_measurements.json').write_text(json.dumps({'source_sha256':'10545968A92A23688162A68D82DCFCF20C10FDD0ABDF3EC58203F6E549D0C3D','events':out},indent=2)+'\n')
print(json.dumps(out,indent=2))

