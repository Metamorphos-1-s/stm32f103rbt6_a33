#!/usr/bin/env python3
import argparse, csv, hashlib, json, math, shutil, struct, subprocess, time
from pathlib import Path

MAGIC = 0x354C4447
VERSION = 3
COMMAND_MAGIC = 0x47574453
CONTROL_WORDS = 22
CONTROL_SIZE = CONTROL_WORDS * 4
SNAPSHOT_PREFIX_WORDS = 30
COUNTER_NAMES = (
    "ready_observation_count", "driver_read_attempt_count",
    "driver_read_success_count", "driver_read_failure_count",
    "driver_timeout_count", "config_write_count", "config_readback_count",
    "config_mismatch_count", "fifo_push_count", "fifo_pop_count",
    "fifo_overrun_count", "measurement_bridge_count",
    "weight_engine_accept_count", "weight_engine_reject_count",
    "sample_sequence_increment_count", "publish_count", "near_rail_count",
    "raw_jump_count", "settling_discard_count",
    "first_ready_timestamp_cycles", "last_ready_timestamp_cycles",
    "minimum_ready_interval_cycles", "maximum_ready_interval_cycles",
    "minimum_read_duration_cycles", "maximum_read_duration_cycles",
    "maximum_fifo_depth")
TRACE_FORMAT = "<IIIiIHBBBBBB"
TRACE_SIZE = struct.calcsize(TRACE_FORMAT)
TRACE_CAPACITY = 16
SNAPSHOT_SIZE = (SNAPSHOT_PREFIX_WORDS + len(COUNTER_NAMES)) * 4 + TRACE_SIZE * TRACE_CAPACITY

CONTROL_NAMES = (
    "magic", "version", "request_sequence", "applied_sequence", "command",
    "requested_mode", "requested_strength", "status", "override_active",
    "effective_mode", "effective_strength", "preserved_dirty",
    "requested_rate", "effective_rate", "rate_override_active", "length",
    "command_magic", "requested_sample_count", "trace_state", "result",
    "trigger_reason", "trace_frozen")
SNAPSHOT_PREFIX_NAMES = (
    "magic", "version", "requested_rate", "expected_config_byte",
    "verified_config_byte", "config_write_accepted",
    "config_readback_verified", "driver_state", "driver_sample_count",
    "processed_sample_count", "read_error_count", "fifo_overrun_count",
    "current_backlog", "maximum_backlog", "event_queue_drop_count",
    "app_run_max_interval_ms", "bridge_max_service_interval_ms",
    "switch_start_ms", "switch_complete_ms", "settling_discarded_samples",
    "last_failure_reason", "restore_expected_config_byte",
    "restore_verified_config_byte", "restore_readback_verified",
    "trace_write_index", "trace_count",
    "raw_anomaly_count", "cpu_clock_hz", "trace_record_version",
    "trace_record_size")
TRACE_NAMES = ("ready_timestamp_cycles", "read_start_cycles",
    "read_done_cycles", "raw", "processed_sequence", "fifo_depth", "flags",
    "rate", "driver_state", "config_register", "config_status", "read_clocks")

def sha256(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def write_json(path, value): Path(path).write_bytes((json.dumps(value, indent=2) + "\n").encode("utf-8"))
def delta32(first, last): return (last - first) & 0xFFFFFFFF

def symbols_from_elf(elf, nm="arm-none-eabi-nm"):
    output = subprocess.check_output([nm, "-S", "--defined-only", str(elf)], text=True)
    result = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[3] in ("g_stage5l_measurement_control", "g_stage5l_rate_diagnostics"):
            result[parts[3]] = {"address": int(parts[0], 16), "size": int(parts[1], 16)}
    if set(result) != {"g_stage5l_measurement_control", "g_stage5l_rate_diagnostics"}:
        raise ValueError("required Stage5L SWD symbols missing")
    if result["g_stage5l_measurement_control"]["size"] != CONTROL_SIZE:
        raise ValueError("control symbol size mismatch")
    if result["g_stage5l_rate_diagnostics"]["size"] != SNAPSHOT_SIZE:
        raise ValueError("snapshot symbol size mismatch")
    return result

def decode_control(data):
    if len(data) != CONTROL_SIZE: raise ValueError("truncated control block")
    value = dict(zip(CONTROL_NAMES, struct.unpack("<%dI" % CONTROL_WORDS, data)))
    if value["magic"] != MAGIC or value["version"] != VERSION or value["length"] != CONTROL_SIZE:
        raise ValueError("control magic/version/length mismatch")
    return value

def decode_snapshot(data):
    if len(data) != SNAPSHOT_SIZE: raise ValueError("truncated diagnostic snapshot")
    words = struct.unpack_from("<%dI" % (SNAPSHOT_PREFIX_WORDS + len(COUNTER_NAMES)), data)
    prefix = dict(zip(SNAPSHOT_PREFIX_NAMES, words[:SNAPSHOT_PREFIX_WORDS]))
    if prefix["magic"] != MAGIC or prefix["version"] != VERSION:
        raise ValueError("snapshot magic/version mismatch")
    if prefix["trace_record_version"] != 1 or prefix["trace_record_size"] != TRACE_SIZE:
        raise ValueError("trace schema/record size mismatch")
    if prefix["trace_count"] > TRACE_CAPACITY or prefix["trace_write_index"] >= TRACE_CAPACITY:
        raise ValueError("trace bounds invalid")
    counters = dict(zip(COUNTER_NAMES, words[SNAPSHOT_PREFIX_WORDS:]))
    trace, offset = [], (SNAPSHOT_PREFIX_WORDS + len(COUNTER_NAMES)) * 4
    for index in range(TRACE_CAPACITY):
        entry = dict(zip(TRACE_NAMES, struct.unpack_from(TRACE_FORMAT, data, offset + index * TRACE_SIZE)))
        entry["index"] = index
        trace.append(entry)
    return {"snapshot": prefix, "counters": counters, "trace": trace}

def ordered_trace(decoded):
    info, trace = decoded["snapshot"], decoded["trace"]
    count, write = info["trace_count"], info["trace_write_index"]
    start = (write - count) % TRACE_CAPACITY
    return [trace[(start + i) % TRACE_CAPACITY] for i in range(count)]

def percentile(values, q):
    if not values: return None
    values = sorted(values); pos = (len(values) - 1) * q
    lo, hi = math.floor(pos), math.ceil(pos)
    return values[lo] if lo == hi else values[lo] + (values[hi] - values[lo]) * (pos - lo)

def analyze(decoded):
    s, c, entries = decoded["snapshot"], decoded["counters"], ordered_trace(decoded)
    clock = s["cpu_clock_hz"]
    span = delta32(c["first_ready_timestamp_cycles"], c["last_ready_timestamp_cycles"])
    rate = ((c["ready_observation_count"] - 1) * clock / span) if c["ready_observation_count"] > 1 and span else None
    intervals = [delta32(a["ready_timestamp_cycles"], b["ready_timestamp_cycles"])
                 for a, b in zip(entries, entries[1:]) if a["ready_timestamp_cycles"] and b["ready_timestamp_cycles"]]
    durations = [delta32(x["read_start_cycles"], x["read_done_cycles"])
                 for x in entries if x["read_start_cycles"] and x["read_done_cycles"]]
    raws = [x["raw"] for x in entries if x["flags"] & 1]
    expected = clock / (40 if s["requested_rate"] == 1 else 10)
    return {"ready_rate_hz": rate, "retained_interval_count": len(intervals),
        "interval_cycles": {"min": min(intervals) if intervals else None,
            "max": max(intervals) if intervals else None,
            "mean": sum(intervals)/len(intervals) if intervals else None,
            "p50": percentile(intervals,.50), "p95": percentile(intervals,.95), "p99": percentile(intervals,.99)},
        "read_duration_cycles": {"min": min(durations) if durations else None,
            "max": max(durations) if durations else None,
            "mean": sum(durations)/len(durations) if durations else None},
        "maximum_continuous_missing_estimate": max(0, round((max(intervals) if intervals else expected)/expected)-1),
        "raw": {"count": len(raws), "min": min(raws) if raws else None,
            "max": max(raws) if raws else None, "mean": sum(raws)/len(raws) if raws else None},
        "layer_deltas": {"ready_minus_read_success": c["ready_observation_count"]-c["driver_read_success_count"],
            "read_success_minus_fifo_push": c["driver_read_success_count"]-c["fifo_push_count"],
            "fifo_push_minus_pop": c["fifo_push_count"]-c["fifo_pop_count"],
            "fifo_pop_minus_engine_accept": c["fifo_pop_count"]-c["weight_engine_accept_count"],
            "engine_accept_minus_sequence": c["weight_engine_accept_count"]-c["sample_sequence_increment_count"]},
        "counters": c}

def parse_map(map_path, symbols):
    return {"path": str(map_path), "sha256": sha256(map_path), "symbols": symbols}

def manifest(run_dir, metadata):
    run = Path(run_dir); files=[]
    for p in sorted(run.iterdir()):
        if p.name == "run_manifest_v2.json" or not p.is_file(): continue
        files.append({"path": p.name, "length": p.stat().st_size, "sha256": sha256(p)})
    value={"schema_version":2,"run_id":run.name,"classification":"STAGE5LR_G2_SWD_EVIDENCE",
        "repository_commit":metadata["repository_commit"],"firmware":metadata["firmware"],"files":files}
    write_json(run/"run_manifest_v2.json",value); return value

def rebind_manifest(run_dir, revision):
    run=Path(run_dir);path=run/"run_manifest_v2.json";value=json.loads(path.read_text(encoding="utf-8"));root=Path(__file__).resolve().parents[2]
    for item in value["files"]:
        relative=(run/item["path"]).resolve().relative_to(root).as_posix()
        data=subprocess.check_output(["git","show",f"{revision}:{relative}"],cwd=root)
        item["length"]=len(data);item["sha256"]=hashlib.sha256(data).hexdigest().upper()
    value["repository_bytes_revision"]=revision
    value["manifest_correction"]="Rebound to committed Git blob bytes; evidence content unchanged"
    write_json(path,value);return value

def decode_files(control_path, snapshot_path, output_dir):
    out=Path(output_dir); out.mkdir(parents=True,exist_ok=True)
    control=decode_control(Path(control_path).read_bytes()); decoded=decode_snapshot(Path(snapshot_path).read_bytes())
    write_json(out/"trace_decoded.json",{"control":control,**decoded})
    with (out/"trace_decoded.csv").open("w",encoding="utf-8",newline="") as f:
        w=csv.DictWriter(f,fieldnames=("index",)+TRACE_NAMES,lineterminator="\n");w.writeheader();w.writerows(ordered_trace(decoded))
    write_json(out/"counter_snapshot.json",decoded["counters"]);write_json(out/"rate_analysis.json",analyze(decoded))
    return control,decoded

def programmer_call(programmer, sn, khz, arguments):
    command=[programmer,"-c","port=SWD","mode=HotPlug",f"freq={khz}",f"sn={sn}"]+arguments
    result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if result.returncode: raise RuntimeError("STM32CubeProgrammer failed:\n"+result.stdout)
    return {"command":command,"output":result.stdout}

def upload(programmer, sn, khz, address, size, path):
    return programmer_call(programmer,sn,khz,["-u",f"0x{address:08X}",str(size),str(Path(path).resolve())])

def write32(programmer, sn, khz, address, value):
    return programmer_call(programmer,sn,khz,["-w32",f"0x{address:08X}",f"0x{value:08X}","-v"])

def git_value(root, *args): return subprocess.check_output(["git",*args],cwd=root,text=True).strip()

def capture_complete(control, mode, sequence):
    exact=control["applied_sequence"]==sequence
    restored_after_capture=(mode=="40" and control["trace_frozen"] and
        not control["rate_override_active"] and control["status"] in (2,4))
    return (exact and ((mode=="restore" and control["status"] in (2,4)) or
        (mode=="10" and control["trace_frozen"]))) or restored_after_capture

def hardware_capture(a):
    out=Path(a.output)
    if out.exists():raise ValueError("output directory already exists")
    root=Path(__file__).resolve().parents[2]
    allowed=(root/"Results"/"stage5lr_g2").resolve()
    resolved_out=out.resolve()
    if allowed not in resolved_out.parents:raise ValueError("hardware evidence output must be under Results/stage5lr_g2")
    if git_value(root,"status","--porcelain","--untracked-files=no"):raise ValueError("hardware evidence requires no tracked worktree changes")
    out.mkdir(parents=True)
    symbols=symbols_from_elf(a.elf,a.nm);control_symbol=symbols["g_stage5l_measurement_control"];snapshot_symbol=symbols["g_stage5l_rate_diagnostics"]
    adapter={"stlink_serial":a.sn,"swd_frequency_khz":a.swd_khz,"programmer":a.programmer,"poll_interval_s":a.poll_interval_s,"continuous_ram_polling":False}
    write_json(out/"swd_adapter.json",adapter)
    before=out/"diagnostic_control_before.bin";log=[];log.append(upload(a.programmer,a.sn,a.swd_khz,control_symbol["address"],CONTROL_SIZE,before));control=decode_control(before.read_bytes())
    write_json(out/"preflight.json",{"utc":time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),"control":control,"symbols":symbols})
    if control["rate_override_active"] or control["trace_state"]==1:raise ValueError("diagnostics already active")
    if a.mode=="10": command,rate,samples=5,0,a.samples
    elif a.mode=="40":
        limit=2400 if a.second_window else 400
        if a.samples>limit:raise ValueError("40 Hz sample limit exceeded")
        command,rate,samples=3,1,a.samples
    else:command,rate,samples=4,0,0
    if a.mode=="10" and samples<256:raise ValueError("10 Hz baseline requires at least 256 samples")
    sequence=(control["request_sequence"]+1)&0xffffffff
    for offset,value in ((48,rate),(68,samples),(16,command),(64,COMMAND_MAGIC),(8,sequence)):
        log.append(write32(a.programmer,a.sn,a.swd_khz,control_symbol["address"]+offset,value))
    deadline=time.monotonic()+a.timeout_s;poll=out/"diagnostic_control_poll.bin";final=None
    while time.monotonic()<deadline:
        time.sleep(a.poll_interval_s);log.append(upload(a.programmer,a.sn,a.swd_khz,control_symbol["address"],CONTROL_SIZE,poll));final=decode_control(poll.read_bytes())
        if capture_complete(final,a.mode,sequence):break
    else:raise TimeoutError("diagnostic command did not complete")
    after=out/"diagnostic_control_after.bin";shutil.copyfile(poll,after);snapshot=out/"diagnostic_snapshot.bin";log.append(upload(a.programmer,a.sn,a.swd_khz,snapshot_symbol["address"],SNAPSHOT_SIZE,snapshot))
    decoded_control,decoded=decode_files(after,snapshot,out);trace_offset=(SNAPSHOT_PREFIX_WORDS+len(COUNTER_NAMES))*4;(out/"trace_raw.bin").write_bytes(snapshot.read_bytes()[trace_offset:])
    head=git_value(root,"rev-parse","HEAD");branch=git_value(root,"branch","--show-current")
    identity={"repository_commit":head,"branch":branch,"worktree":"clean","elf":{"path":str(a.elf),"length":Path(a.elf).stat().st_size,"sha256":sha256(a.elf)},"map":{"path":str(a.map),"length":Path(a.map).stat().st_size,"sha256":sha256(a.map)},"symbols":symbols,"control_schema":VERSION,"trace_schema":1,"trace_record_size":TRACE_SIZE}
    write_json(out/"firmware_identity.json",identity);write_json(out/"postflight.json",{"utc":time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),"control":decoded_control,"snapshot":decoded["snapshot"]});write_json(out/"swd_operations.json",log)
    manifest(out,{"repository_commit":head,"firmware":identity});print("SWD CAPTURE COMPLETE",out.name);return 0

def main():
    p=argparse.ArgumentParser(); sub=p.add_subparsers(dest="command",required=True)
    s=sub.add_parser("inspect-elf");s.add_argument("--elf",required=True);s.add_argument("--map",required=True);s.add_argument("--output",required=True);s.add_argument("--nm",default="arm-none-eabi-nm")
    s=sub.add_parser("decode");s.add_argument("--control",required=True);s.add_argument("--snapshot",required=True);s.add_argument("--output",required=True)
    s=sub.add_parser("validate-manifest");s.add_argument("--input",required=True)
    s=sub.add_parser("rebind-manifest");s.add_argument("--input",required=True);s.add_argument("--git-revision",required=True)
    s=sub.add_parser("capture");s.add_argument("--mode",choices=("10","40","restore"),required=True);s.add_argument("--samples",type=int,default=256);s.add_argument("--second-window",action="store_true");s.add_argument("--output",required=True);s.add_argument("--elf",required=True);s.add_argument("--map",required=True);s.add_argument("--sn",required=True);s.add_argument("--swd-khz",type=int,default=1800);s.add_argument("--poll-interval-s",type=float,default=2.0);s.add_argument("--timeout-s",type=float,default=90.0);s.add_argument("--programmer",default="STM32_Programmer_CLI.exe");s.add_argument("--nm",default="arm-none-eabi-nm")
    a=p.parse_args()
    if a.command=="inspect-elf":
        syms=symbols_from_elf(a.elf,a.nm);write_json(a.output,{"elf":{"path":a.elf,"length":Path(a.elf).stat().st_size,"sha256":sha256(a.elf)},"map":parse_map(a.map,syms)});return 0
    if a.command=="decode":decode_files(a.control,a.snapshot,a.output);return 0
    if a.command=="capture":return hardware_capture(a)
    if a.command=="rebind-manifest":rebind_manifest(a.input,a.git_revision);return 0
    run=Path(a.input);m=json.loads((run/"run_manifest_v2.json").read_text(encoding="utf-8"))
    if m["run_id"]!=run.name:raise ValueError("run id mismatch")
    for f in m["files"]:
        q=run/f["path"]
        if not q.exists() or q.stat().st_size!=f["length"] or sha256(q)!=f["sha256"]:raise ValueError("manifest mismatch: "+f["path"])
        if q.suffix==".json":json.loads(q.read_text(encoding="utf-8"))
    print("MANIFEST V2 PASS",run.name);return 0
if __name__=="__main__":raise SystemExit(main())
