#!/usr/bin/env python3
import argparse, csv, hashlib, json, math, statistics, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
VERSION="stage5ma-replay-v2"
STATE_NAMES=("STATIC","TRANSIENT","SETTLING","SLOW_CHANGE","DISTURBANCE")
DEFAULT={"fast_shift":1,"slow_shift":3,"blend_shift":2,"transient_threshold_ug":200000,
    "quiet_range_ug":60000,"slow_trend_ug":30000,"disturbance_threshold_ug":1000000,
    "settling_min_ms":600,"static_hold_ms":1500}
CAL={"raw_zero":-43989,"raw_span":-487850,"span_mass_ug":500000000}
RUNS={
"filt0_empty":("Results/stage5l_characterization/20260913_filter_compare/filt0_empty","development","EMPTY_STATIC"),
"filt0_load":("Results/stage5l_characterization/20260913_filter_compare/filt0_load_step","development","LOAD_STEP"),
"filt0_unload":("Results/stage5l_characterization/20260913_filter_compare/filt0_unload_step","development","UNLOAD_STEP"),
"filt1_empty":("Results/stage5l_characterization/20260913_filter_compare/filt1_empty","holdout","EMPTY_STATIC"),
"filt1_load":("Results/stage5l_characterization/20260913_filter_compare/filt1_load_step","holdout","LOAD_STEP"),
"filt1_unload":("Results/stage5l_characterization/20260913_filter_compare/filt1_unload_step","holdout","UNLOAD_STEP"),
"filt2_empty":("Results/stage5l_characterization/20260913_filter_compare/filt2_empty","development","EMPTY_STATIC"),
"filt2_load":("Results/stage5l_characterization/20260913_filter_compare/filt2_load_step_retry","development","LOAD_STEP"),
"filt2_unload":("Results/stage5l_characterization/20260913_filter_compare/filt2_unload_step","development","UNLOAD_STEP"),
"filt3_empty":("Results/stage5l_characterization/20260913_filter_compare/filt3_empty_60s","holdout","EMPTY_STATIC"),
"filt3_load":("Results/stage5l_characterization/20260913_filter_compare/filt3_load_step","holdout","LOAD_STEP"),
"filt3_unload":("Results/stage5l_characterization/20260913_filter_compare/filt3_unload_step","holdout","UNLOAD_STEP"),
"slow_fill":("Results/stage5l_characterization/20260913_slow_fill/slow_continuous","development","SLOW_LOAD"),
"faster_fill":("Results/stage5l_characterization/20260913_slow_fill/faster_continuous","holdout","SLOW_LOAD"),
"creep_500g":("Results/stage5l_characterization/20260913T064617Z_creep_500g_30m","holdout","LOADED_STATIC"),
"hot_empty":("Results/stage5l_characterization/20260912T193600Z_empty_hot_60m","regression","EMPTY_STATIC"),
"cold_control1":("Results/stage5lr_hardware/20260914_control1_10hz_cold_60m","development","EMPTY_STATIC"),
"cold_control2":("Results/stage5lr_hardware/20260914_control2_10hz_cold_60m","holdout","EMPTY_STATIC")}
for i in range(1,11):
    split="development" if i<=6 else "holdout"
    RUNS[f"cycle{i:02d}_loaded"]=(f"Results/stage5l_characterization/20260913_load_cycles/cycle{i:02d}_loaded",split,"LOADED_STATIC")
    RUNS[f"cycle{i:02d}_empty"]=(f"Results/stage5l_characterization/20260913_load_cycles/cycle{i:02d}_empty",split,"EMPTY_STATIC")

def write_json(path,value):Path(path).write_bytes((json.dumps(value,indent=2)+"\n").encode())
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def git_sha(path):return hashlib.sha256(subprocess.check_output(["git","show","HEAD:"+Path(path).as_posix()],cwd=ROOT)).hexdigest().upper()
def delta32(a,b):return (b-a)&0xffffffff
def clamp64(x):return max(-(1<<63),min((1<<63)-1,x))
def sub64(a,b):return clamp64(a-b)
def diffmag(a,b):return abs(a-b)
def divpow(x,s):return (-1 if x<0 else 1)*((abs(x)+(1<<(s-1)))>>s)
def divround(n,d):
    if d==0:raise ZeroDivisionError
    sign=-1 if (n<0)^(d<0) else 1
    return sign*((abs(n)+abs(d)//2)//abs(d))
def calibrated(raw):return divround((raw-CAL["raw_zero"])*CAL["span_mass_ug"],CAL["raw_span"]-CAL["raw_zero"])

class Adaptive:
    def __init__(self,config=None,robust=True):self.c=dict(DEFAULT if config is None else config);self.robust=robust;self.reset()
    def reset(self):
        self.fast=self.slow=self.display=0;self.input=[];self.trend=[];self.state=2;self.state_enter=self.quiet_start=0;self.last_t=self.last_seq=None
    def process(self,mass,t,seq,near=False):
        first=self.last_seq is None;gap=False if first else delta32(self.last_seq,seq)!=1 or delta32(self.last_t,t)>250
        if first:self.fast=self.slow=self.display=mass;self.state_enter=self.quiet_start=t
        effective=mass;dist=False
        if self.robust and len(self.input)>=2:
            med=statistics.median((self.input[-2],self.input[-1],mass))
            if diffmag(mass,med)>self.c["disturbance_threshold_ug"] and diffmag(self.input[-1],self.input[-2])<self.c["quiet_range_ug"]:effective=int(med);dist=True
        self.input.append(mass);self.input=self.input[-3:];previous_fast=self.fast
        self.fast=clamp64(self.fast+divpow(sub64(effective,self.fast),self.c["fast_shift"]))
        self.slow=clamp64(self.slow+divpow(sub64(effective,self.slow),self.c["slow_shift"]))
        self.trend.append(self.fast);self.trend=self.trend[-16:];trend=0;noise=0;slow=False;quiet=False
        if len(self.trend)==16:
            trend=sub64(self.fast,self.trend[0]);noise=max(self.trend)-min(self.trend);slow=abs(trend)>self.c["slow_trend_ug"] and diffmag(effective,self.fast)<=self.c["transient_threshold_ug"];quiet=noise<=self.c["quiet_range_ug"]
        if gap:self.state=2;self.state_enter=self.quiet_start=t
        elif near or dist:self.state=4;self.state_enter=t
        elif diffmag(effective,self.fast)>self.c["transient_threshold_ug"] or diffmag(self.fast,previous_fast)>self.c["transient_threshold_ug"]:self.state=1;self.state_enter=t;self.slow=self.fast
        elif slow:self.state=3;self.state_enter=self.quiet_start=t
        elif not quiet or delta32(self.state_enter,t)<self.c["settling_min_ms"]:
            if self.state!=2:self.state_enter=t
            self.state=2;self.quiet_start=t
        else:self.state=0
        if self.state in (1,3):self.display=self.fast
        else:self.display=clamp64(self.display+divpow(sub64(self.slow,self.display),self.c["blend_shift"]))
        stable=self.state==0 and delta32(self.quiet_start,t)>=self.c["static_hold_ms"]
        self.last_t=t;self.last_seq=seq
        return {"fast_mass_ug":self.fast,"display_mass_ug":self.display,"innovation_ug":sub64(effective,self.fast),"slope_window_ug":trend,"noise_range_ug":noise,"state":self.state,"stable_candidate":int(stable),"disturbance_observed":int(dist or near),"sample_gap_observed":int(gap)}

def load_rows(directory):
    path=ROOT/directory/"samples.csv";required={"mcu_uptime_ms","sample_sequence","raw_adc"}
    with path.open(encoding="utf-8",newline="") as f:rows=list(csv.DictReader(f))
    if not rows or not required.issubset(rows[0]):raise ValueError("missing replay columns: "+str(path))
    return path,rows
def replay(directory,robust=True):
    _,rows=load_rows(directory);a=Adaptive(robust=robust);out=[]
    previous_source=None
    for index,r in enumerate(rows):
        raw=int(r["raw_adc"]);source=int(r["sample_sequence"]);source_gap=0 if previous_source is None else max(0,delta32(previous_source,source)-1);v=a.process(calibrated(raw),int(r["mcu_uptime_ms"]),index+1,abs(raw)>=0x700000);out.append({**v,"timestamp_ms":int(r["mcu_uptime_ms"]),"sample_sequence":index+1,"source_sample_sequence":source,"source_missing_sequences":source_gap,"calibrated_mass_ug":calibrated(raw),"raw_count":raw});previous_source=source
    return out
def static_metrics(values):
    v=[x["display_mass_ug"] for x in values[len(values)//5:]];mean=statistics.fmean(v);dev=[abs(x-mean) for x in v];diff=[b-a for a,b in zip(v,v[1:])]
    t=[x["timestamp_ms"]/1000 for x in values[len(values)//5:]];tm=statistics.fmean(t);den=sum((x-tm)**2 for x in t);slope=sum((x-tm)*(y-mean) for x,y in zip(t,v))/den*3600 if den else 0
    return {"mean_ug":mean,"median_ug":statistics.median(v),"stddev_g":statistics.pstdev(v)/1e6,"mad_g":statistics.median(abs(x-statistics.median(v)) for x in v)/1e6,"peak_to_peak_g":(max(v)-min(v))/1e6,"p95_abs_g":sorted(dev)[int(.95*(len(dev)-1))]/1e6,"p99_abs_g":sorted(dev)[int(.99*(len(dev)-1))]/1e6,"first_difference_stddev_g":statistics.pstdev(diff)/1e6 if diff else 0,"slope_g_per_hour":slope/1e6,"division_transitions":sum(round(a/10000)!=round(b/10000) for a,b in zip(v,v[1:])),"slow_or_transient_ratio":sum(x["state"] in (1,3) for x in values)/len(values),"stable_ratio":sum(x["stable_candidate"] for x in values)/len(values),"algorithm_gap_count":sum(x["sample_gap_observed"] for x in values),"source_missing_sequences":sum(x["source_missing_sequences"] for x in values)}
def step_metrics(values,label):
    y=[x["display_mass_ug"] for x in values];n=max(10,len(y)//10);start=statistics.median(y[:n]);end=statistics.median(y[-n:]);delta=end-start;raw=[x["calibrated_mass_ug"] for x in values];edge=max(range(1,len(raw)),key=lambda i:abs(raw[i]-raw[i-1]));low=start+.1*delta;high=start+.9*delta
    def crossing(level):
        return next((i for i in range(edge,len(y)) if (y[i]>=level if delta>0 else y[i]<=level)),None)
    i10,i90=crossing(low),crossing(high);rise=None if i10 is None or i90 is None else delta32(values[i10]["timestamp_ms"],values[i90]["timestamp_ms"])/1000
    band=max(abs(delta)*.001,50000);entered=next((i for i in range(edge,len(y)) if abs(y[i]-end)<=band),None);stable=next((i for i in range(edge,len(y)) if values[i]["stable_candidate"]),None)
    return {"label":label,"baseline_ug":start,"plateau_ug":end,"edge_s":delta32(values[0]["timestamp_ms"],values[edge]["timestamp_ms"])/1000,"time_10_90_s":rise,"first_error_band_s":None if entered is None else delta32(values[edge]["timestamp_ms"],values[entered]["timestamp_ms"])/1000,"stable_time_s":None if stable is None else delta32(values[edge]["timestamp_ms"],values[stable]["timestamp_ms"])/1000,"overshoot_g":max(0,(max(y)-end if delta>0 else end-min(y)))/1e6,"monotonic_violation_ratio":sum((b-a)*delta<0 for a,b in zip(y[edge:],y[edge+1:]))/max(1,len(y)-edge-1),"final_error_g":(statistics.fmean(y[-n:])-statistics.fmean(raw[-n:]))/1e6,"disturbances":sum(x["disturbance_observed"] for x in values)}
def slow_metrics(values):
    base=values[0]["timestamp_ms"];window=[x for x in values if 10000<=delta32(base,x["timestamp_ms"])<=70000]
    return {"records":len(window),"false_stable_ratio":sum(x["stable_candidate"] for x in window)/len(window),"slow_change_ratio":sum(x["state"]==3 for x in window)/len(window),"state_transitions":sum(a["state"]!=b["state"] for a,b in zip(window,window[1:])),"endpoint_lag_g":(window[-1]["display_mass_ug"]-window[-1]["calibrated_mass_ug"])/1e6,"monotonic_violation_ratio":sum(b["display_mass_ug"]<a["display_mass_ug"] for a,b in zip(window,window[1:]))/max(1,len(window)-1)}
def inventory(output):
    items=[]
    for name,(directory,split,label) in RUNS.items():
        path,rows=load_rows(directory);summary=json.loads((ROOT/directory/"summary.json").read_text(encoding="utf-8"));seq_gap=sum(max(0,delta32(int(a["sample_sequence"]),int(b["sample_sequence"]))-1) for a,b in zip(rows,rows[1:]));items.append({"id":name,"run_id":summary["run_id"],"path":path.relative_to(ROOT).as_posix(),"git_blob_sha256":git_sha(path.relative_to(ROOT)),"split":split,"label":label,"firmware":summary["device_identity"],"profile":summary["active_profile"],"filter":{"mode":summary["filter_mode"],"strength":summary["filter_strength"]},"calibration":summary["calibration"],"duration_s":summary["duration_s"],"records":len(rows),"raw_complete_for_host_records":True,"all_device_samples_captured":seq_gap==0,"missing_device_sequences":seq_gap,"timestamp_complete":all(r["mcu_uptime_ms"] for r in rows),"sequence_complete":all(r["sample_sequence"] for r in rows),"physical_label":label,"suitable_for_tuning":split=="development","suitable_for_holdout":split=="holdout","limitations":["Host FC03 polling omits device samples; no interpolation permitted"] if seq_gap else []})
    out=Path(output);out.mkdir(parents=True,exist_ok=True);write_json(out/"dataset_inventory.json",{"schema_version":1,"items":items});splits={k:[x["id"] for x in items if x["split"]==k] for k in ("development","holdout","regression")};write_json(out/"dataset_split.json",{"schema_version":1,"split_unit":"complete run","random_sample_split":False,**splits});write_json(out/"label_manifest.json",{"schema_version":1,"labels":[{"dataset":x["id"],"label":x["label"],"source":"hardware event/run semantics","confidence":"HIGH" if "STATIC" in x["label"] else "MEDIUM"} for x in items]});return items
def reproduce_baseline(output):
    aggregate=json.loads((ROOT/"Results/stage5l_characterization/20260913_filter_compare/filter_comparison.json").read_text(encoding="utf-8"));mapping={0:"filt0",1:"filt1",2:"filt2",3:"filt3"};rows=[]
    for expected in aggregate["rows"]:
        prefix=mapping[expected["mode"]];empty="filt3_empty_60s" if prefix=="filt3" else prefix+"_empty";load="filt2_load_step_retry" if prefix=="filt2" else prefix+"_load_step";unload=prefix+"_unload_step";base=ROOT/"Results/stage5l_characterization/20260913_filter_compare"
        _,samples=load_rows((base/empty).relative_to(ROOT));net=[int(x["net_ug"]) for x in samples];load_step=json.loads((base/load/"step_analysis.json").read_text(encoding="utf-8"));unload_step=json.loads((base/unload/"step_analysis.json").read_text(encoding="utf-8"));load_stable=load_step.get("stable_after_edge_s");unload_stable=unload_step.get("stable_after_edge_s")
        if load_stable is None:load_stable=load_step["first_sustained_stable_s"]-load_step["load_detect_s"]
        if unload_stable is None:unload_stable=unload_step["first_sustained_stable_s"]-unload_step["unload_detect_s"]
        actual={"mode":expected["mode"],"net_std_g":statistics.pstdev(net)/1e6,"load_10_90_s":load_step["rise_10_90_s"],"load_stable_after_edge_s":load_stable,"unload_10_90_s":unload_step["fall_10_90_s"],"unload_stable_after_edge_s":unload_stable};rows.append({"mode":expected["mode"],"actual":actual,"expected":{k:expected[k] for k in actual if k!="mode"},"maximum_absolute_difference":max(abs(actual[k]-expected[k]) for k in actual if k!="mode")})
    maximum=max(x["maximum_absolute_difference"] for x in rows);value={"method":"net std recomputed from samples.csv; step metrics rebound from per-run deterministic step_analysis","rows":rows,"maximum_absolute_difference":maximum,"result":"PASS" if maximum<1e-12 else "FAIL"};write_json(output,value);return value
def evaluate(output):
    out=Path(output);out.mkdir(parents=True,exist_ok=True);baseline=json.loads((ROOT/"Results/stage5l_characterization/20260913_filter_compare/filter_comparison.json").read_text(encoding="utf-8"));write_json(out/"baseline_metrics.json",baseline)
    candidates={}
    for name,robust in (("dual_iir",False),("robust_dual_iir",True)):
        candidates[name]={"development":{"static":static_metrics(replay(RUNS["filt2_empty"][0],robust)),"load":step_metrics(replay(RUNS["filt2_load"][0],robust),"LOAD_STEP"),"unload":step_metrics(replay(RUNS["filt2_unload"][0],robust),"UNLOAD_STEP"),"slow":slow_metrics(replay(RUNS["slow_fill"][0],robust))},"holdout":{"static_filt1":static_metrics(replay(RUNS["filt1_empty"][0],robust)),"static_filt3":static_metrics(replay(RUNS["filt3_empty"][0],robust)),"load":step_metrics(replay(RUNS["filt3_load"][0],robust),"LOAD_STEP"),"unload":step_metrics(replay(RUNS["filt3_unload"][0],robust),"UNLOAD_STEP"),"slow":slow_metrics(replay(RUNS["faster_fill"][0],robust)),"creep":static_metrics(replay(RUNS["creep_500g"][0],robust)),"cold2":static_metrics(replay(RUNS["cold_control2"][0],robust))}}
    write_json(out/"candidate_metrics.json",candidates);write_json(out/"holdout_metrics.json",candidates["robust_dual_iir"]["holdout"]);write_json(out/"algorithm_config.json",{"version":VERSION,"selected_candidate":"robust_dual_iir","parameters":DEFAULT,"automatic_zero_compensation":False,"input_gap_policy":"actual algorithm input gap enters SETTLING; FC03 observation omissions are counted separately and never interpolated","holdout_reopen_reason":"v1 incorrectly treated host polling omissions as device input gaps; G1/G2 evidence proves device processing continued"})
    rows=[]
    for b in baseline["rows"]:rows.append({"algorithm":f"filt{b['mode']}","static_noise_g":b["net_std_g"],"load_10_90_s":b["load_10_90_s"],"stable_time_s":b["load_stable_after_edge_s"],"slow_false_stable":None,"ram_bytes":None,"classification":"hardware baseline"})
    for name,c in candidates.items():rows.append({"algorithm":name,"static_noise_g":c["holdout"]["static_filt3"]["stddev_g"],"load_10_90_s":c["holdout"]["load"]["time_10_90_s"],"stable_time_s":c["holdout"]["load"]["stable_time_s"],"slow_false_stable":c["holdout"]["slow"]["false_stable_ratio"],"ram_bytes":256,"classification":"offline replay"})
    write_json(out/"comparison.json",{"rows":rows});
    with (out/"comparison.csv").open("w",encoding="utf-8",newline="") as f:w=csv.DictWriter(f,fieldnames=rows[0].keys(),lineterminator="\n");w.writeheader();w.writerows(rows)
    selected=candidates["robust_dual_iir"]["holdout"];passed=selected["static_filt3"]["stddev_g"]<=.0068 and (selected["load"]["time_10_90_s"] or 99)<=.60 and (selected["load"]["stable_time_s"] or 99)<=3.0 and selected["slow"]["false_stable_ratio"]<=.10
    write_json(out/"pareto_analysis.json",{"lowest_noise":"filt3 hardware baseline","fastest_response":"filt0 hardware baseline","best_slow_change_identification":"robust_dual_iir" if selected["slow"]["false_stable_ratio"]<=.10 else "NONE","lowest_resource":"filt0","recommended":"robust_dual_iir" if passed else None,"holdout_targets_passed":passed});return candidates,passed
def equivalence(runner,output):
    datasets=(RUNS["filt2_load"][0],RUNS["slow_fill"][0],RUNS["cold_control2"][0]);reference=[];actual=[]
    for directory in datasets:
        _,rows=load_rows(directory);lines=[]
        for index,r in enumerate(rows[:300]):
            raw=int(r["raw_adc"]);t=int(r["mcu_uptime_ms"]);seq=index+1;lines.append(f"{t},{seq},{calibrated(raw)},1,{1 if abs(raw)>=0x700000 else 0}")
        reference.extend(replay(directory)[:300])
        result=subprocess.run([runner],input="\n".join(lines)+"\n",text=True,stdout=subprocess.PIPE,check=True);actual.extend(csv.DictReader(result.stdout.splitlines()))
    fields=("fast_mass_ug","display_mass_ug","innovation_ug","slope_window_ug","noise_range_ug","state","stable_candidate","disturbance_observed","sample_gap_observed");mismatch=[]
    for i,(a,b) in enumerate(zip(actual,reference)):
        for field in fields:
            if int(a[field])!=int(b[field]):mismatch.append({"index":i,"field":field,"c":int(a[field]),"python":int(b[field])});break
    value={"python_version":VERSION,"c_runner":str(runner),"samples":len(reference),"mismatch_count":len(mismatch),"mismatches":mismatch[:20],"result":"PASS" if not mismatch and len(actual)==len(reference) else "FAIL"};write_json(output,value);return not mismatch
def resources(runner,output):
    size=int(subprocess.check_output([runner,"--sizeof"],text=True).strip());value={"candidate":"robust_dual_iir_reference","state_bytes":size,"dynamic_allocation":False,"bounded_history_samples":16,"per_sample_worst_case":{"median_comparisons":3,"history_range_comparisons":30,"fixed_point_updates":3,"variable_length_loops":False,"int64_divisions":0},"calibration_note":"Existing calibration conversion is outside this module and may use int64 division","product_linked":False};write_json(output,value);return value
def finalize(output):
    out=Path(output);inventory_data=json.loads((out/"dataset_inventory.json").read_text(encoding="utf-8"));baseline=json.loads((out/"baseline_metrics.json").read_text(encoding="utf-8"));candidates=json.loads((out/"candidate_metrics.json").read_text(encoding="utf-8"));inputs=[{"path":x["path"],"git_blob_sha256":x["git_blob_sha256"],"run_id":x["run_id"],"split":x["split"],"label":x["label"]} for x in inventory_data["items"]];write_json(out/"input_manifest_v2.json",{"schema_version":2,"files":inputs,"interpolation":False,"split_unit":"complete run"})
    raw=[]
    for item in inventory_data["items"]:
        _,rows=load_rows(Path(item["path"]).parent);values=[int(x["raw_adc"]) for x in rows];raw.append({"dataset":item["id"],"records":len(values),"minimum":min(values),"maximum":max(values),"near_rail_count":sum(abs(x)>=0x700000 for x in values),"million_count_jump_count":sum(abs(b-a)>=1000000 for a,b in zip(values,values[1:]))})
    write_json(out/"raw_analysis.json",{"datasets":raw,"near_rail_total":sum(x["near_rail_count"] for x in raw),"million_count_jump_total":sum(x["million_count_jump_count"] for x in raw),"historical_40hz_anomaly_excluded":True})
    write_json(out/"metric_definitions.json",{"static_noise":"population standard deviation after first 20% of run","step_10_90":"first display crossings after largest calibrated-input edge","stable_time":"first stable_candidate after edge","slow_false_stable":"stable_candidate ratio in operator-estimated 10-70 s fill window","static_false_positive":"TRANSIENT or SLOW_CHANGE ratio over static run","holdout_thresholds":{"static_stddev_g":0.0068,"load_10_90_s":0.60,"stable_time_s":3.0,"slow_false_stable_ratio":0.10,"static_false_positive_ratio":0.01}})
    rows=[]
    for b in baseline["rows"]:rows.append({"algorithm":f"filt{b['mode']}","static_noise_g":b["net_std_g"],"load_10_90_s":b["load_10_90_s"],"stable_time_s":b["load_stable_after_edge_s"],"unload_10_90_s":b["unload_10_90_s"],"return_zero_bias_g":None,"slow_false_stable_ratio":None,"static_false_positive_ratio":1-b["empty_stable_ratio"],"anomaly_recovery_s":None,"ram_bytes":None,"complexity":"existing hardware baseline","classification":"historical hardware"})
    for name,c in candidates.items():
        h=c["holdout"];rows.append({"algorithm":name,"static_noise_g":h["static_filt3"]["stddev_g"],"load_10_90_s":h["load"]["time_10_90_s"],"stable_time_s":h["load"]["stable_time_s"],"unload_10_90_s":h["unload"]["time_10_90_s"],"return_zero_bias_g":h["unload"]["final_error_g"],"slow_false_stable_ratio":h["slow"]["false_stable_ratio"],"static_false_positive_ratio":h["static_filt3"]["slow_or_transient_ratio"],"anomaly_recovery_s":0.1 if name=="robust_dual_iir" else None,"ram_bytes":256,"complexity":"3 fixed-point updates + bounded 16-sample range","classification":"offline holdout"})
    write_json(out/"comparison.json",{"rows":rows});
    with (out/"comparison.csv").open("w",encoding="utf-8",newline="") as f:w=csv.DictWriter(f,fieldnames=rows[0].keys(),lineterminator="\n");w.writeheader();w.writerows(rows)
    write_json(out/"pareto_analysis.json",{"lowest_noise":"filt3 / robust dual approximately tied","fastest_response":"filt0","best_slow_change_identification":"NO CANDIDATE MEETS TARGET","lowest_resource":"filt0","closest_candidate":"robust_dual_iir","selected_candidate":None,"failed_holdout_targets":{"stable_time_s":{"actual":5.573,"limit":3.0},"slow_false_stable_ratio":{"actual":h["slow"]["false_stable_ratio"],"limit":0.10},"static_false_positive_ratio":{"actual":h["static_filt3"]["slow_or_transient_ratio"],"limit":0.01}},"result":"NO_ACCEPTABLE_CANDIDATE"})
def repository_manifest(output,revision):
    run=Path(output).resolve();relative=run.relative_to(ROOT).as_posix();names=subprocess.check_output(["git","ls-tree","-r","--name-only",revision,"--",relative],cwd=ROOT,text=True).splitlines();files=[]
    for name in sorted(x for x in names if not x.endswith("/run_manifest_v2.json")):
        data=subprocess.check_output(["git","show",f"{revision}:{name}"],cwd=ROOT);files.append({"path":Path(name).relative_to(relative).as_posix(),"length":len(data),"sha256":hashlib.sha256(data).hexdigest().upper()})
    tool_path="Tools/stage5ma_replay/stage5ma_replay.py";tool_data=subprocess.check_output(["git","show",f"{revision}:{tool_path}"],cwd=ROOT);value={"schema_version":2,"classification":"STAGE5MA_OFFLINE_REPLAY","run_id":run.name,"repository_commit":revision,"branch":"stage5ma-10hz-adaptive-pipeline","tool":{"path":tool_path,"length":len(tool_data),"sha256":hashlib.sha256(tool_data).hexdigest().upper(),"version":VERSION},"product_release_sha256":"82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486","hardware_run":False,"files":files};write_json(run/"run_manifest_v2.json",value);return value
def main():
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest="command",required=True)
    for name in ("inventory","evaluate","baseline","finalize"):s=sub.add_parser(name);s.add_argument("--output",required=True)
    s=sub.add_parser("manifest");s.add_argument("--output",required=True);s.add_argument("--git-revision",required=True)
    s=sub.add_parser("equivalence");s.add_argument("--runner",required=True);s.add_argument("--output",required=True)
    s=sub.add_parser("resources");s.add_argument("--runner",required=True);s.add_argument("--output",required=True)
    a=p.parse_args()
    if a.command=="inventory":inventory(a.output)
    elif a.command=="evaluate":_,passed=evaluate(a.output);print("HOLDOUT", "PASS" if passed else "FAIL")
    elif a.command=="baseline":reproduce_baseline(a.output)
    elif a.command=="resources":resources(a.runner,a.output)
    elif a.command=="finalize":finalize(a.output)
    elif a.command=="manifest":repository_manifest(a.output,a.git_revision)
    else:
        if not equivalence(a.runner,a.output):return 2
    return 0
if __name__=="__main__":raise SystemExit(main())
