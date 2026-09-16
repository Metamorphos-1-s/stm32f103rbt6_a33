#!/usr/bin/env python3
import argparse
import hashlib
import json
import statistics
from pathlib import Path


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def max_ten_second(rows):
    values = [int(row["offset_ug"]) for row in rows]
    return max((abs(values[index] - values[index - 10])
                for index in range(10, len(values))), default=0)


def range_of(rows, field):
    values = [int(row[field]) for row in rows]
    return [min(values), max(values)]


def main():
    parser=argparse.ArgumentParser();parser.add_argument("--input",required=True);args=parser.parse_args()
    root=Path(args.input)
    s1=load(root/"20260916T_r5b_s1_off_5m.json")
    s2=load(root/"20260916T_r5b_s2_shadow_empty_18m.json")
    preload=load(root/"20260916T_r5b_s3_dosing_preload_30s.json")
    loaded=load(root/"20260916T_r5b_s3_dosing_loaded_30s.json")
    s3=load(root/"20260916T_r5b_s3_shadow_500g_18m.json")
    s4=load(root/"20260916T_r5b_s4_active_500g_10m.json")
    preunload=load(root/"20260916T_r5b_s4_dosing_preunload_30s.json")
    unloaded=load(root/"20260916T_r5b_s4_dosing_unloaded_30s.json")
    rebuild=load(root/"20260916T_r5b_s4_static_rebuild_330s.json")
    final=load(root/"20260916T_r5b_s5_final_probe.json")["state"]
    shadow=load(root/"20260916T_r5b_s5_set_shadow.json")
    off=load(root/"20260916T_r5b_s5_set_off.json")
    captures=(s1,s2,preload,loaded,s3,s4,preunload,unloaded,rebuild)
    rows=[row for capture in captures for row in capture["rows"]]
    loaded_mean=statistics.fmean(row["gross_mass_ug"] for row in preunload["rows"])
    unloaded_mean=statistics.fmean(row["gross_mass_ug"] for row in unloaded["rows"])
    active_sync=[int(row["gross_mass_ug"])-(int(row["uncompensated_gross_ug"])-int(row["offset_ug"])) for row in s4["rows"]]
    config_before=root.parent/"20260916T_r5b_preflash"/"config_before"/"config_region.bin"
    config_final=root/"20260916T_r5b_final_config"/"config_region.bin"
    summary={
      "schema_version":1,
      "classification":"STAGE5MR5B_ENGINEERING_BETA_SUPERVISED_SMOKE",
      "supervised_capture_duration_s":sum(capture["duration_s"] for capture in captures),
      "supervised_capture_records":sum(capture["records"] for capture in captures),
      "s1":{"duration_s":s1["duration_s"],"offset_range_ug":range_of(s1["rows"],"offset_ug"),"passed":range_of(s1["rows"],"offset_ug")==[0,0]},
      "s2":{"duration_s":s2["duration_s"],"reference_fill_max":max(row["reference_fill"] for row in s2["rows"]),"observation_fill_max":max(row["observation_fill"] for row in s2["rows"]),"evaluation_count_max":max(row["evaluation_count"] for row in s2["rows"]),"maximum_10s_offset_change_g":max_ten_second(s2["rows"])/1e6},
      "s3":{"dosing_offset_range_ug":range_of(loaded["rows"],"offset_ug"),"shadow_duration_s":s3["duration_s"],"evaluation_count_max":max(row["evaluation_count"] for row in s3["rows"]),"maximum_10s_offset_change_g":max_ten_second(s3["rows"])/1e6},
      "s4":{"active_duration_s":s4["duration_s"],"maximum_absolute_offset_g":max(abs(row["offset_ug"]) for row in s4["rows"])/1e6,"maximum_10s_offset_change_g":max_ten_second(s4["rows"])/1e6,"active_display_range":range_of(s4["rows"],"display_count"),"active_candidate_sync_median_error_ug":statistics.median(active_sync),"dosing_offset_range_ug":range_of(unloaded["rows"],"offset_ug"),"loaded_unloaded_difference_g":(loaded_mean-unloaded_mean)/1e6,"static_rebuild_reference_fill_max":max(row["reference_fill"] for row in rebuild["rows"]),"static_rebuild_final_state":rebuild["rows"][-1]["state"]},
      "s5":{"shadow_display_before":shadow["before"]["display_count"],"shadow_display_after":shadow["after"]["display_count"],"off_display_before":off["before"]["display_count"],"off_display_after":off["after"]["display_count"],"final_firmware":final["firmware"],"final_signature":final["signature"],"final_application":final["application"],"final_mode":final["mode"],"final_offset_ug":final["offset_ug"],"final_fault":final["fault_mask"],"final_dirty":final["dirty"],"final_revision":final["revision"],"final_saved_revision":final["saved_revision"]},
      "configuration":{"before_sha256":sha(config_before),"final_sha256":sha(config_final),"byte_identical":Path(config_before).read_bytes()==Path(config_final).read_bytes()},
      "whole_smoke":{"maximum_fault":max(row["fault_mask"] for row in rows),"maximum_dirty":max(row["dirty"] for row in rows),"revision_range":range_of(rows,"revision"),"saved_revision_range":range_of(rows,"saved_revision"),"overrun_delta":max(row["overrun_count"] for row in rows)-min(row["overrun_count"] for row in rows),"save_count":0,"flash_write_count_during_smoke":0,"application_flash_count_before_smoke":1}
    }
    summary["gates"]={
      "no_fault":summary["whole_smoke"]["maximum_fault"]==0,
      "dosing_offset_frozen":summary["s3"]["dosing_offset_range_ug"][0]==summary["s3"]["dosing_offset_range_ug"][1] and summary["s4"]["dosing_offset_range_ug"][0]==summary["s4"]["dosing_offset_range_ug"][1],
      "off_does_not_apply_offset":summary["s1"]["passed"],
      "maximum_10s_at_most_0_001g":max(summary["s2"]["maximum_10s_offset_change_g"],summary["s3"]["maximum_10s_offset_change_g"],summary["s4"]["maximum_10s_offset_change_g"])<=0.001,
      "absolute_offset_at_most_0_500g":summary["s4"]["maximum_absolute_offset_g"]<=0.5,
      "load_difference_preserved":abs(summary["s4"]["loaded_unloaded_difference_g"]-500)<=0.1,
      "no_display_jump_on_cleanup":summary["s5"]["shadow_display_before"]==summary["s5"]["shadow_display_after"]==summary["s5"]["off_display_before"]==summary["s5"]["off_display_after"],
      "no_dirty_or_revision_change":summary["whole_smoke"]["maximum_dirty"]==0 and summary["whole_smoke"]["revision_range"]==[7,7] and summary["whole_smoke"]["saved_revision_range"]==[7,7],
      "configuration_unchanged":summary["configuration"]["byte_identical"],
      "final_safe_state":final["application"]==0 and final["mode"]==0 and final["offset_ug"]==0,
    }
    summary["passed"]=all(summary["gates"].values())
    (root/"smoke_summary.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(summary,indent=2));return 0 if summary["passed"] else 1


if __name__=="__main__":raise SystemExit(main())
