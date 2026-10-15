"""Summarize 0x052C display-chain capture without inferring unseen MCU samples."""
import argparse, csv, hashlib, json
from pathlib import Path

def main():
    p=argparse.ArgumentParser(); p.add_argument("csv_path",type=Path); p.add_argument("--output",type=Path)
    a=p.parse_args(); rows=list(csv.DictReader(a.csv_path.open(newline="",encoding="utf-8")))
    changes=[]
    for left,right in zip(rows,rows[1:]):
        if left["display_count"] != right["display_count"]:
            changes.append({"utc":right["utc"],"sequence":int(right["sequence"]),"old":int(left["display_count"]),"new":int(right["display_count"]),"state":int(right["display_state"]),"reason":int(right["release_reason"]),"stable":int(right["stable"]),"raw":int(right["raw_adc"]),"filtered":int(right["filtered_raw"]),"net":int(right["net_ug"])})
    result={"source":str(a.csv_path),"sha256":hashlib.sha256(a.csv_path.read_bytes()).hexdigest().upper(),"rows":len(rows),"display_count_changes":len(changes),"unique_display_counts":len(set(row["display_count"] for row in rows),),"states":sorted(set(int(row["display_state"]) for row in rows)),"release_reasons":sorted(set(int(row["release_reason"]) for row in rows)),"stable_rows":sum(int(row["stable"]) for row in rows),"read_errors":len((a.csv_path.parent/"errors.jsonl").read_text().splitlines()),"first_changes":changes[:20],"last_changes":changes[-20:]}
    text=json.dumps(result,indent=2)+"\n"
    if a.output:a.output.write_text(text,encoding="utf-8")
    print(text)
if __name__=="__main__":main()
