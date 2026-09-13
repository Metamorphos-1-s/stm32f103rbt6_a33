#!/usr/bin/env python3
import argparse, csv, hashlib, json, platform, statistics, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import ModbusClient
from serial_transport import SerialTransport

EVENTS = {"POWER_ON","WARMUP_START","EMPTY","LOAD_START","LOAD_PLACED","LOAD_STABLE","UNLOAD_START","EMPTY_AGAIN","SLOW_FILL_START","SLOW_FILL_STOP","TEST_END"}

def u32(w): return (w[0]<<16)|w[1]
def i32(w):
    v=u32(w); return v-(1<<32) if v&(1<<31) else v
def u64(w): return (w[0]<<48)|(w[1]<<32)|(w[2]<<16)|w[3]
def i64(w):
    v=u64(w); return v-(1<<64) if v&(1<<63) else v
def sequence_gap(previous, current):
    gap=(current-previous-1)&0xffffffff
    return gap if gap<0x80000000 else 0
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def atomic_json(path, value):
    p=Path(path); tmp=p.with_suffix(p.suffix+".tmp"); tmp.write_text(json.dumps(value,indent=2)+"\n",encoding="utf-8"); tmp.replace(p)

def decode(primary, display, drift, host_utc, host_ns, filter_mode, filter_strength, sample_rate):
    flags=primary[4]|(primary[5]<<16); decimals=primary[2]
    return {"utc":host_utc,"host_monotonic_ns":host_ns,"mcu_uptime_ms":u32(primary[0x22:0x24]),"sample_sequence":u32(primary[0x20:0x22]),"raw_adc":i32(primary[0x1c:0x1e]),"filtered_raw":i32(primary[0x1e:0x20]),"raw_calibrated_mass_ug":"","uncompensated_gross_ug":i64(drift[8:12]),"filtered_mass_ug":i64(primary[0x14:0x18]),"pre_display_mass_ug":i64(primary[0x10:0x14]) if primary[0x0d]==0 else i64(primary[0x14:0x18]),"conditioned_display_mass_ug":i64(display[2:6]),"display_count":i32(primary[0:2]),"display_decimals":decimals,"gross_ug":i64(primary[0x14:0x18]),"net_ug":i64(primary[0x10:0x14]),"tare_ug":i64(primary[0x18:0x1c]),"stable":1 if flags&(1<<4) else 0,"zero":1 if flags&(1<<5) else 0,"overload":1 if flags&(1<<7) else 0,"zero_offset_raw":"","runtime_drift_offset_ug":i64(drift[4:8]),"filter_mode":filter_mode,"filter_strength":filter_strength,"sample_rate":sample_rate,"gain":primary[0x2a],"cs1237_state":primary[0x2b],"buffered_samples":primary[0x2c],"overrun_count":u32(primary[0x2d:0x2f]),"stability_spread_ug":i64(primary[0x24:0x28]),"battery_voltage_mv":"","fault_mask":u32(primary[0x39:0x3b]),"display_condition_state":display[0],"display_locked":display[1],"event_label":""}

def capture(args):
    out=Path(args.output); out.mkdir(parents=True,exist_ok=False); run_id=out.name; tool=Path(__file__).resolve()
    tr=SerialTransport(args.port,args.baud,args.parity,args.stopbits,args.timeout_ms); c=ModbusClient(tr,args.slave)
    fields=None; rows=0; duplicates=0; observed_gaps=0; last_seq=None; start=time.time(); start_ns=time.monotonic_ns(); display=[0]*17; drift=[0]*30
    events=out/"events.jsonl"; samples=out/"samples.csv"
    try:
        identity=c.read(14,2)[0]; schema=c.read(0x13e,1)[0][0]; fmt=c.read(0x1c0,1)[0][0]; active=[]
        for a in range(0x100,0x140,16): active+=c.read(a,16)[0]
        calibration=c.read(0x190,10)[0]; profile=active[31]; base=32 if profile==0 else 46; filter_mode=active[base+2]; filter_strength=active[base+3]; sample_rate=active[base]; override=None; rate_override=None
        if args.override_evidence:
            override_path=Path(args.override_evidence);override=json.loads(override_path.read_text(encoding='utf-8'));control=override.get('after',{})
            if override.get('action')!='apply' or control.get('status')!=1 or control.get('override_active')!=1: raise RuntimeError('invalid active override evidence')
            filter_mode=control['effective_mode'];filter_strength=control['effective_strength']
        if args.rate_override_evidence:
            rate_path=Path(args.rate_override_evidence);rate_override=json.loads(rate_path.read_text(encoding='utf-8'));control=rate_override.get('after',{})
            if rate_override.get('action')!='apply-rate' or control.get('status')!=1 or control.get('rate_override_active')!=1:raise RuntimeError('invalid active rate override evidence')
            sample_rate=control['effective_rate']
        if identity!=[0x0104,0x0510] or schema!=2 or fmt!=3: raise RuntimeError("device identity mismatch")
        with events.open("w",encoding="utf-8") as ef:
            ef.write(json.dumps({"utc":time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),"event":args.start_event,"run_id":run_id})+"\n")
        with samples.open("w",newline="",encoding="utf-8") as sf:
            writer=None; next_aux=0
            while time.time()-start < args.duration_s:
                cycle=time.monotonic()
                primary=c.read(0,64)[0]
                seq=u32(primary[0x20:0x22])
                if seq==last_seq: duplicates+=1; continue
                if last_seq is not None:
                    observed_gaps+=sequence_gap(last_seq,seq)
                last_seq=seq
                if cycle>=next_aux:
                    display=c.read(0x1e0,17)[0]; drift=c.read(0x200,30)[0]; next_aux=cycle+args.aux_interval_s
                row=decode(primary,display,drift,time.strftime('%Y-%m-%dT%H:%M:%S',time.gmtime())+f'.{int(time.time()%1*1000):03d}Z',time.monotonic_ns(),filter_mode,filter_strength,sample_rate)
                if writer is None: fields=list(row); writer=csv.DictWriter(sf,fieldnames=fields); writer.writeheader()
                writer.writerow(row); sf.flush(); rows+=1
                delay=args.poll_interval_s-(time.monotonic()-cycle)
                if delay>0: time.sleep(delay)
        with events.open("a",encoding="utf-8") as ef: ef.write(json.dumps({"utc":time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),"event":args.end_event,"run_id":run_id})+"\n")
    finally: tr.close()
    end=time.time(); head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(); elf=ROOT/'build'/'Release'/'stm32f103rbt6_a33.elf'
    env={"run_id":run_id,"repository_head":head,"python":sys.version,"platform":platform.platform(),"tool_path":str(tool.relative_to(ROOT)).replace('\\','/'),"tool_length":tool.stat().st_size,"tool_sha256":sha(tool),"firmware_elf_path":str(elf.relative_to(ROOT)).replace('\\','/') if elf.exists() else None,"firmware_elf_length":elf.stat().st_size if elf.exists() else None,"firmware_elf_sha256":sha(elf) if elf.exists() else None,"port":args.port,"baud":args.baud,"parity":args.parity,"stopbits":args.stopbits,"slave":args.slave}
    summary={"run_id":run_id,"test_kind":args.command,"started_utc":time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime(start)),"ended_utc":time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime(end)),"duration_s":end-start,"records":rows,"host_duplicate_polls":duplicates,"host_observation_missing_sequences":observed_gaps,"device_identity":{"firmware":"0x0510","map":"0x0104","public_schema":2,"persistent_format":3},"capacity_ug":i64(active[4:8]),"calibration":{"raw_zero":i32(calibration[0:2]),"raw_span":i32(calibration[2:4]),"span_mass_ug":i64(calibration[4:8]),"sequence":u32(calibration[8:10])},"active_profile":profile,"filter_mode":filter_mode,"filter_strength":filter_strength,"sample_rate":sample_rate,"diagnostic_override_evidence":None if override is None else {"path":args.override_evidence,"sha256":sha(args.override_evidence),"control":override['after']},"diagnostic_rate_evidence":None if rate_override is None else {"path":args.rate_override_evidence,"sha256":sha(args.rate_override_evidence),"control":rate_override['after']},"active_config":active,"writes":0,"flash_operations":0,"result":"EVIDENCE_ONLY"}
    atomic_json(out/"environment.json",env); atomic_json(out/"summary.json",summary); analyze_dir(out); build_manifest(out); return 0

def slope(values,times):
    if len(values)<2:return 0.0
    mt=sum(times)/len(times); mv=sum(values)/len(values); den=sum((x-mt)**2 for x in times)
    return 0.0 if den==0 else sum((x-mt)*(y-mv) for x,y in zip(times,values))/den
def analyze_dir(out):
    with (Path(out)/"samples.csv").open(encoding="utf-8") as stream: rows=list(csv.DictReader(stream))
    if not rows: raise ValueError("empty samples")
    t=[int(r["mcu_uptime_ms"])/1000 for r in rows]; result={"records":len(rows)}
    for key in ("raw_adc","filtered_raw","uncompensated_gross_ug","gross_ug","net_ug","conditioned_display_mass_ug","display_count"):
        v=[float(r[key]) for r in rows]; result[key]={"mean":statistics.fmean(v),"stddev":statistics.pstdev(v),"peak_to_peak":max(v)-min(v),"linear_drift_per_hour":slope(v,t)*3600}
    result["stable_ratio"]=sum(int(r["stable"]) for r in rows)/len(rows); result["device_overrun_delta"]=int(rows[-1]["overrun_count"])-int(rows[0]["overrun_count"])
    result["multi_scale_mean_difference"]={}
    for seconds in (1,10,60):
        buckets={}
        for x,y in zip(t,[float(r["net_ug"]) for r in rows]): buckets.setdefault(int((x-t[0])//seconds),[]).append(y)
        means=[statistics.fmean(v) for _,v in sorted(buckets.items())]
        result["multi_scale_mean_difference"][str(seconds)+"s"]=statistics.pstdev(means) if len(means)>1 else 0.0
    atomic_json(Path(out)/"analysis.json",result); return result
def build_manifest(out):
    out=Path(out); files=[]
    for name in ("samples.csv","events.jsonl","environment.json","summary.json","analysis.json"):
        p=out/name; files.append({"path":name,"length":p.stat().st_size,"sha256":sha(p)})
    atomic_json(out/"manifest.json",{"schema_version":1,"run_id":out.name,"files":files})
def validate(out):
    out=Path(out); m=json.loads((out/"manifest.json").read_text(encoding="utf-8"))
    if m["run_id"]!=out.name: raise ValueError("run id mismatch")
    for f in m["files"]:
        p=out/f["path"]
        if not p.exists() or p.stat().st_size!=f["length"] or sha(p)!=f["sha256"]: raise ValueError("manifest mismatch: "+f["path"])
        if p.suffix==".json": json.loads(p.read_text(encoding="utf-8"))
    print("MANIFEST PASS",out.name); return 0
def parser():
    p=argparse.ArgumentParser(); sub=p.add_subparsers(dest="command",required=True)
    for name in ("capture-baseline","capture-step","capture-creep","capture-zero-return","capture-slow-ramp","capture-rate-compare"):
        s=sub.add_parser(name); s.add_argument('--port',required=True); s.add_argument('--duration-s',type=float,required=True); s.add_argument('--output',required=True); s.add_argument('--baud',type=int,default=115200); s.add_argument('--parity',default='N'); s.add_argument('--stopbits',type=int,default=1); s.add_argument('--slave',type=int,default=1); s.add_argument('--timeout-ms',type=int,default=300); s.add_argument('--poll-interval-s',type=float,default=0.0); s.add_argument('--aux-interval-s',type=float,default=1.0); s.add_argument('--override-evidence'); s.add_argument('--rate-override-evidence'); s.add_argument('--start-event',choices=sorted(EVENTS),default='EMPTY'); s.add_argument('--end-event',choices=sorted(EVENTS),default='TEST_END')
    s=sub.add_parser('analyze'); s.add_argument('--input',required=True)
    s=sub.add_parser('validate-manifest'); s.add_argument('--input',required=True); return p
def main():
    a=parser().parse_args()
    if a.command=='analyze': analyze_dir(Path(a.input)); build_manifest(Path(a.input)); return 0
    if a.command=='validate-manifest': return validate(a.input)
    return capture(a)
if __name__=='__main__': raise SystemExit(main())
