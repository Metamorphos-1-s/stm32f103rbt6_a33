#!/usr/bin/env python3
import argparse,csv,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).resolve().parent))
from display_model import CandidateConfig,IncrementalDisplay,quantize_count
from evaluate_candidates import RUNS,rows
FIELDS=("desired_display_count","actual_display_count","anchor_count","confirmation_count","release_reason","locked","large_step")
def run_case(runner,name,records):
    model=IncrementalDisplay(CandidateConfig(1,1000));lines=[];expected=[]
    for row in records:
        desired=quantize_count(int(row["net_mass_ug"]),1000000,100,1);baseline=int(row["display_count"]);now=int(row["uptime_ms"]);stable=(int(row["status_flags"])&16)!=0;active=int(row["application"])==1
        p=model.process(desired,now,stable,active,baseline_count=baseline);expected.append(p);lines.append(f"{desired},{baseline},{now},0,{int(stable)},{int(active)},1")
    result=subprocess.run([str(runner)],input="\n".join(lines)+"\n",text=True,capture_output=True,check=True);actual=list(csv.DictReader(result.stdout.splitlines()));m=[]
    for index,(a,e) in enumerate(zip(actual,expected)):
        if any(int(a[f])!=int(e[f]) for f in FIELDS):m.append({"index":index,"actual":a,"expected":e})
    return {"case":name,"samples":len(expected),"mismatches":len(m),"first":m[:10]}
def main():
    p=argparse.ArgumentParser();p.add_argument("--runner",required=True);p.add_argument("--output",required=True);a=p.parse_args();runner=Path(a.runner);cases=[]
    for name,path in RUNS.items():cases.append(run_case(runner,name,list(rows(path))))
    value={"schema_version":1,"state_bytes":int(subprocess.check_output([str(runner),"--sizeof"],text=True)),"cases":cases,"samples":sum(x["samples"] for x in cases),"mismatches":sum(x["mismatches"] for x in cases)};Path(a.output).write_text(json.dumps(value,indent=2)+"\n",encoding="utf-8");print(json.dumps(value,indent=2));return 0 if value["mismatches"]==0 else 2
if __name__=="__main__":raise SystemExit(main())
