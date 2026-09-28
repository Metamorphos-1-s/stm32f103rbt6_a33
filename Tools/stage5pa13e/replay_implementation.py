"""Regression of the unchanged C math on already-open A13C-R sample/mode history."""
import argparse
import csv
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/stage5pa13c'))
from check_parity import compare,sha
sys.path.insert(0,str(ROOT/'Tools/stage5pa13cr'))
from analyze_focused import analyze

def main():
    p=argparse.ArgumentParser();p.add_argument('--runner',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    source=ROOT/'Results/stage5pa13c_r/static_load_unload_continuous/samples.csv'
    expected='499D2F14D2E2A04FEE27369209610089AA39D91CD39B1CBABD04A27B8D2CA838'
    if sha(source)!=expected:raise ValueError('original hardware SHA differs')
    segments=[];part=[];last=None
    with source.open(encoding='utf-8',newline='') as stream:
        for row in csv.DictReader(stream):
            seq=int(row['candidate_sequence'])
            if seq==last:continue
            last=seq;mode=int(row['mode'])
            if not mode:
                if part:segments.append(part);part=[]
                continue
            part.append((seq,int(row['candidate_mcu_ms']),int(row['host_monotonic_ns']),int(row['candidate_uncompensated_ug']),mode))
    if part:segments.append(part)
    comparisons=[]
    all_pass=True
    for i,segment in enumerate(segments):
        # DOSING intervals come from observed device modes, never user markers.
        intervals=[(item[2],segment[j+1][2] if j+1<len(segment) else item[2]+1)
                   for j,item in enumerate(segment) if item[4]==1]
        output=a.output/f'c_python_segment_{i}.json'
        passed=compare([item[:4] for item in segment],intervals,a.runner,output)
        all_pass=all_pass and passed
        comparisons.append(json.loads(output.read_text()))
    measured=analyze(source)
    if measured['live_python_parity']['mismatches']:raise ValueError('recorded MCU/Python mismatch')
    result=dict(status='PASS' if all_pass else 'FAIL',source=str(source),sha256=expected,
        C_python_samples=sum(len(s) for s in segments),comparisons=comparisons,
        measured_python_parity=measured['live_python_parity'],
        identity='IMPLEMENTATION REGRESSION; not independent efficacy',
        reset_contract='new instance only across actual OFF; continuous across STATIC/DOSING')
    (a.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='comparisons'}))
    if not all_pass:raise SystemExit(1)
if __name__=='__main__':main()
