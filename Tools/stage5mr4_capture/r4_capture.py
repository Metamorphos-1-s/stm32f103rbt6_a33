#!/usr/bin/env python3
import argparse, hashlib, json, subprocess, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CAPTURE=ROOT/"Tools"/"stage5l_measurement"/"stage5l_capture.py"

def write_json(path,value):Path(path).write_bytes((json.dumps(value,indent=2)+"\n").encode("utf-8"))
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def main():
 p=argparse.ArgumentParser();p.add_argument("--port",required=True);p.add_argument("--duration-s",type=float,required=True);p.add_argument("--output",required=True);p.add_argument("--kind",choices=("cold_empty","hot_empty","constant_load","load_cycle"),required=True);p.add_argument("--sensor-label",required=True);p.add_argument("--physical-rated-capacity-kg",type=float);p.add_argument("--known-load-g",type=float);p.add_argument("--start-event",default="EMPTY");a=p.parse_args();out=Path(a.output)
 if out.exists():raise ValueError("output exists")
 command=[sys.executable,str(CAPTURE),"capture-baseline","--port",a.port,"--duration-s",str(a.duration_s),"--output",str(out),"--start-event",a.start_event,"--end-event","TEST_END"]
 subprocess.run(command,cwd=ROOT,check=True)
 summary=json.loads((out/"summary.json").read_text(encoding="utf-8"));meta={"run_id":out.name,"r4_kind":a.kind,"sensor_label":a.sensor_label,"physical_rated_capacity_kg":a.physical_rated_capacity_kg,"configured_capacity_kg":summary["capacity_ug"]/1e9,"known_load_g":a.known_load_g,"r3_frozen_parameters":{"observation_window_s":180,"block_median_s":15,"estimator":"whole-window OLS","deadband_g":0.002,"declared_static_ceiling_g_per_h":1.0,"fast_step_g":0.020,"correction_cap_g_per_min":0.0045,"correction_cap_ug_per_s":75,"trend_damping_permille":875,"mode_reenable_holdoff_s":15},"capture_tool":{"path":CAPTURE.relative_to(ROOT).as_posix(),"sha256":sha(CAPTURE)},"writes":0,"flash_operations":0};write_json(out/"r4_metadata.json",meta)
 subprocess.run([sys.executable,str(CAPTURE),"analyze","--input",str(out)],cwd=ROOT,check=True);return 0
if __name__=="__main__":raise SystemExit(main())
