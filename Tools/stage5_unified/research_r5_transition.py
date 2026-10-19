"""Synthetic 40 Hz R5 transition study through the real host measurement path.

No serial imports, no hardware replay, no inference of missing ADC samples.
The screen compares alternatives against frozen 0x0530, not metrology limits.
"""
import argparse
import csv
import gzip
import hashlib
import io
import json
import math
import statistics
import subprocess
from pathlib import Path

BASE_COMMIT = "e4a0b5161688a4ada95880befc2fb33013407536"
POLICIES = ("baseline", "hold60", "hold120", "paired_reference")
RATE = 40
LOAD_S, UNLOAD_S, END_S = 240, 600, 1200
NOISE_UG = (0, 1000, -1000, 2000, -2000, 3000, -3000)
RAW_ZERO, RAW_SPAN, SPAN_UG = 41868, 485780, 500_000_000
TOLERANCE_UG = math.ceil(2 * SPAN_UG / (RAW_SPAN - RAW_ZERO))
SOURCE_PINS = {
    "Domain/measurement/a13c_shadow_compensator.c": "BF0F828E5DB4E9837EC051E14E8D39F78D8885159061F91CDEB278A6DEA1DE2F",
    "App/metrology_manager.c": "3160225B13071F018BA3CACECDE6FAFE66F43D078BE274F4AFD5A3C389E6DF98",
    "Domain/measurement/weight_engine.c": "358A69C94718924C82F1D9515BF34033A4D52FBD46164B20C279AE49033334A4",
    "Domain/filter/weight_filter.c": "F1FACE407EAD83D3D97895AA07AB44CB12E16245CC8E5A625B065E903A19C42E",
    "Domain/calibration/calibration_model.c": "E149773F940688582921744DE2D7B73A08DF154FBEA4D1DBA9A93531B545E375",
}


def source_identity(root):
    result = {}
    for name, expected in SOURCE_PINS.items():
        # Git text blobs may be checked out as CRLF on the local Windows host.
        actual = hashlib.sha256((root / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest().upper()
        if actual != expected:
            raise ValueError(f"frozen source mismatch: {name}")
        result[name] = actual
    return result


def scenarios():
    result = [dict(name=f"global_drift_{sign}", kind="drift", sign=sign)
              for sign in (-1, 1)]
    result += [dict(name=f"load_creep_{sign}", kind="creep", sign=sign)
               for sign in (-1, 1)]
    for residual in (20_000, 40_000, 80_000):
        for kind in ("mechanical_residual", "real_residue"):
            result.append(dict(name=f"{kind}_{residual}", kind=kind, residual_ug=residual))
    for drop in (20_000, 50_000, 100_000):
        for dosing in (False, True):
            mode = "dosing" if dosing else "static"
            result.append(dict(name=f"drops_{drop}_{mode}", kind="drops", drop_ug=drop, dosing=dosing))
    return result


def drops_before(second):
    # Three 30-drop runs, with 2 s and 10 s pauses, one drop each second.
    return sum(max(0, min(30, math.floor(second - start) + 1)) for start in (360, 392, 432))


def sample(spec, sequence):
    second = (sequence - 1) / RATE
    true_mass = 500_000_000 if LOAD_S <= second < UNLOAD_S else 0
    sensor = 380_000
    mode = 2
    if spec["kind"] == "drift":
        sensor += spec["sign"] * round(max(0, second - LOAD_S) * 100)
    elif spec["kind"] == "creep":
        if LOAD_S <= second < UNLOAD_S:
            sensor += spec["sign"] * round(80_000 * (1 - math.exp(-(second - LOAD_S) / 80)))
        elif second >= UNLOAD_S:
            sensor += spec["sign"] * round(40_000 * math.exp(-(second - UNLOAD_S) / 180))
    elif spec["kind"] in ("mechanical_residual", "real_residue"):
        if second >= UNLOAD_S:
            if spec["kind"] == "real_residue":
                true_mass += spec["residual_ug"]
            else:
                sensor += spec["residual_ug"]
    elif spec["kind"] == "drops":
        true_mass += drops_before(second) * spec["drop_ug"]
        if spec["dosing"] and 350 <= second < 480:
            mode = 1
    measured = true_mass + sensor + NOISE_UG[(sequence - 1) % len(NOISE_UG)]
    raw = RAW_ZERO + round(measured * (RAW_SPAN - RAW_ZERO) / SPAN_UG)
    return raw, mode, true_mass


def make_input(spec):
    values = [sample(spec, sequence) for sequence in range(1, RATE * END_S + 1)]
    data = "".join(f"{raw},{mode}\n" for raw, mode, _ in values).encode("ascii")
    return data, [mass for _, _, mass in values]


def compress(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as output, gzip.GzipFile(fileobj=output, mode="wb", mtime=0) as stream:
        stream.write(data)


def parse_output(data):
    reader = csv.DictReader(io.StringIO(data.decode("ascii")))
    rows = [{key: int(value) for key, value in row.items()} for row in reader]
    if len(rows) != RATE * END_S:
        raise ValueError("runner omitted physical samples")
    for index, row in enumerate(rows, 1):
        if row["sequence"] != index or row["limited"]:
            raise ValueError("runner sequence/limited failure")
        if index > 100 and (row["application"] != 1 or
                row["net_ug"] != row["uncompensated_ug"] - row["offset_ug"]):
            raise ValueError("authority equation/application failure")
    return rows


def window(rows, truth, start, end):
    first, last = round(start * RATE), round(end * RATE)
    data, mass = rows[first:last], truth[first:last]
    if last - first != len(data) or not data:
        raise ValueError("incomplete synthetic window")
    return dict(net_median_ug=statistics.median(r["net_ug"] for r in data),
        sensor_error_median_ug=statistics.median(r["net_ug"] - t for r, t in zip(data, mass)),
        truth_median_ug=statistics.median(mass),
        offset_median_ug=statistics.median(r["offset_ug"] for r in data))


def metrics(rows, truth, spec):
    before = window(rows, truth, LOAD_S - 45, LOAD_S - 15)
    result = dict(pre=before, return_windows={}, loaded_windows={})
    for low, high in ((15, 45), (60, 90), (90, 120), (270, 300), (570, 600)):
        w = window(rows, truth, UNLOAD_S + low, UNLOAD_S + high)
        w["return_error_ug"] = w["sensor_error_median_ug"] - before["sensor_error_median_ug"]
        w["apparent_zero_shift_ug"] = w["net_median_ug"] - before["net_median_ug"]
        result["return_windows"][f"{low}_{high}"] = w
    for low, high in ((90, 120), (270, 300)):
        w = window(rows, truth, LOAD_S + low, LOAD_S + high)
        w["load_error_ug"] = w["sensor_error_median_ug"] - before["sensor_error_median_ug"]
        result["loaded_windows"][f"{low}_{high}"] = w
    if spec["kind"] == "drops":
        pre_dose = window(rows, truth, 330, 350)
        dose = {}
        for name, start, end in (("pause2", 391, 392), ("pause10", 430, 432), ("retained", 555, 585)):
            w = window(rows, truth, start, end)
            w["added_truth_ug"] = w["truth_median_ug"] - pre_dose["truth_median_ug"]
            w["added_error_ug"] = ((w["net_median_ug"] - pre_dose["net_median_ug"]) -
                                     w["added_truth_ug"])
            dose[name] = w
        result["dose"] = dose
    return result


def score_comparisons(all_results):
    summary = {}
    for policy in POLICIES[1:]:
        regressions, improvements = [], []
        for name, by_policy in all_results.items():
            baseline, candidate = by_policy["baseline"]["metrics"], by_policy[policy]["metrics"]
            for group, field in (("return_windows", "return_error_ug"),
                                 ("loaded_windows", "load_error_ug"), ("dose", "added_error_ug")):
                for key, value in baseline.get(group, {}).items():
                    before, after = abs(value[field]), abs(candidate[group][key][field])
                    item = dict(scenario=name, metric=f"{group}/{key}", baseline_abs_ug=before,
                                candidate_abs_ug=after, degradation_ug=after - before)
                    if after > before + TOLERANCE_UG:
                        regressions.append(item)
                    elif before > after + TOLERANCE_UG:
                        improvements.append(item)
        summary[policy] = dict(status="REJECTED_SCREEN_REGRESSION" if regressions else
            "ELIGIBLE_FOR_FURTHER_REVIEW" if improvements else "NO_MATERIAL_IMPROVEMENT",
            regressions=regressions, improvements=improvements)
    return summary


def write_summary(path, results):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["scenario", "policy", "group", "window", "truth_ug", "error_ug", "offset_ug"])
        for scenario, by_policy in results.items():
            for policy, run in by_policy.items():
                for group, field in (("return_windows", "return_error_ug"),
                                     ("loaded_windows", "load_error_ug"), ("dose", "added_error_ug")):
                    for key, value in run["metrics"].get(group, {}).items():
                        writer.writerow([scenario, policy, group, key, value["truth_median_ug"],
                                         value[field], value["offset_median_ug"]])


def historical_carry(path):
    """Same-input arithmetic only, never a sampled or interpolated replay."""
    from audit_zero_return_atomic_0530 import find_edges, window as atomic_window
    expected = "20574BEC82C8F5154E3CB7A4D07D2CBA2C43781ED51A877D4B736BBF7E33D9A2"
    actual = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    if actual != expected:
        raise ValueError("historical capture hash mismatch")
    rows = [json.loads(line) for line in path.open()]
    edges = find_edges(rows)
    cycles = []
    if len(edges) != 6:
        raise ValueError("historical edge count changed")
    for load, unload in zip(edges[::2], edges[1::2]):
        if (load["direction"], unload["direction"]) != ("LOAD", "UNLOAD"):
            raise ValueError("historical event order changed")
        pre = atomic_window(rows, load["after"]["host_monotonic_ns"], -45, -15)
        post = atomic_window(rows, unload["after"]["host_monotonic_ns"], 15, 45)
        if not pre["complete"] or not post["complete"]:
            raise ValueError("incomplete historical comparison")
        load_offset = load["after"]["authority"]["applied_offset_ug"]
        unload_offset = unload["after"]["authority"]["applied_offset_ug"]
        cycles.append(dict(load_edge_offset_ug=load_offset, unload_edge_offset_ug=unload_offset,
            offset_accumulated_between_edges_ug=unload_offset - load_offset,
            pre_offset_mean_ug=pre["stats"]["applied_offset_ug"]["mean_ug"],
            observed_early_return_mean_ug=post["stats"]["net_ug"]["mean_ug"] - pre["stats"]["net_ug"]["mean_ug"],
            same_input_restore_pre_mean_offset_residual_ug=
                post["stats"]["uncompensated_ug"]["mean_ug"] - pre["stats"]["uncompensated_ug"]["mean_ug"]))
    return dict(evidence_commit="f5137c0f1eb6171d4f2e459bd28539f51fb8896f",sha256=actual,
        scope="observed fixed trace plus arithmetic offset substitution, not an OFF experiment or replay", cycles=cycles)


def run(build, output):
    identity = source_identity(Path(__file__).resolve().parents[2])
    if output.exists():
        raise ValueError("output must be a new directory")
    output.mkdir(parents=True)
    specs = scenarios()
    # Written before any run; do not choose thresholds after seeing results.
    preregistered = dict(base_commit=BASE_COMMIT, protocol_revision=2,
        revision_note="Initial 14-scenario screen expanded with matched DOSING controls for 20/100 mg drops; no screening threshold changed.",
        physical_hz=RATE, sample_period_ms=25, source_pins=identity,
        canonical_csv_newlines="LF",
        controller_admission="actual MetrologyManager 40-to-10 Hz adapter", policies=POLICIES,
        load_s=LOAD_S, unload_s=UNLOAD_S, duration_s=END_S,
        tolerance_ug=TOLERANCE_UG, tolerance_scope="two raw counts; research comparison only, not metrology acceptance",
        scenario_count=len(specs), scenarios=specs,
        drop_sizes_scope="synthetic 20/50/100 mg steps; no assertion of physical droplet mass")
    (output / "preregistered.json").write_text(json.dumps(preregistered, indent=2) + "\n")
    results = {}
    for spec in specs:
        source, truth = make_input(spec)
        name = spec["name"]
        compress(output / name / "input.csv.gz", source)
        results[name] = {}
        for policy in POLICIES:
            executable = build / f"stage5p_r5_transition_{policy}"
            if not executable.exists():
                executable = executable.with_suffix(".exe")
            proc = subprocess.run([str(executable.resolve())], input=source, capture_output=True, timeout=180)
            if proc.returncode:
                raise RuntimeError(f"{name}/{policy}: exit {proc.returncode}: {proc.stderr[-1000:]!r}")
            canonical = proc.stdout.replace(b"\r\n", b"\n")
            rows = parse_output(canonical)
            compress(output / name / f"{policy}.csv.gz", canonical)
            results[name][policy] = dict(input_sha256=hashlib.sha256(source).hexdigest().upper(),
                output_sha256=hashlib.sha256(canonical).hexdigest().upper(),
                physical_rows=len(rows), metrics=metrics(rows, truth, spec))
            print(json.dumps(dict(scenario=name, policy=policy, rows=len(rows))), flush=True)
    identical = []
    for residual in (20_000, 40_000, 80_000):
        left, right = f"mechanical_residual_{residual}", f"real_residue_{residual}"
        for policy in POLICIES:
            a, b = results[left][policy], results[right][policy]
            if a["input_sha256"] != b["input_sha256"] or a["output_sha256"] != b["output_sha256"]:
                raise ValueError("identical-input counterexample was not identical")
        identical.append(dict(scenarios=[left, right], indistinguishable_for_all_policies=True,
                              different_required_true_mass_ug=residual))
    result = dict(preregistered=preregistered, results=results, identical_input_pairs=identical,
        screen=score_comparisons(results),
        classification="HOST_SYNTHETIC_SCREEN_NOT_HARDWARE_OR_METROLOGY_QUALIFICATION")
    (output / "analysis.json").write_text(json.dumps(result, indent=2) + "\n")
    write_summary(output / "summary.csv", results)
    print(json.dumps({p: v["status"] for p, v in result["screen"].items()}), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.build, args.output)


if __name__ == "__main__":
    main()
