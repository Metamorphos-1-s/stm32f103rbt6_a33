"""Offline, decimated/non-atomic snapshot replay; never opens a device.

Builds the same adapter against frozen 0x052E and the working candidate.
Assumes the archived capture's g/2 decimals/division=1 configuration.
No missing 40 Hz conversions are interpolated. Output is not panel evidence.
"""
import argparse
import csv
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "3937faa46d5b2433fedc8b44f8b4d3328c80b8ee"
DEFS = [
    "STAGE5MR5_BETA", "STAGE5NB_BETA", "STAGE5MR5E_D1D_BETA",
    "STAGE5PA_PRODUCT", "STAGE5PA2D_CALIBRATION", "STAGE5PA13C_SHADOW",
    "STAGE5PA13E_ACTIVE", "STAGE5P_UNIFIED", "STAGE5P_DISPLAY_SMOOTH",
    "STAGE5P_PANEL_BATTERY_ZERO_LONGRATE",
]

def build(cc, output, baseline):
    source = ROOT / "Domain/measurement/display_conditioner.c"
    if baseline:
        source = output.parent / "display_conditioner_052e.c"
        source.write_bytes(subprocess.check_output(
            ["git", "show", BASELINE + ":Domain/measurement/display_conditioner.c"],
            cwd=ROOT))
    command = [cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    command += ["-DA33_ENABLE_" + name + "=1" for name in DEFS]
    command += ["-I" + str(ROOT / "Config"),
                "-I" + str(ROOT / "Domain/measurement")]
    command += [str(source), str(ROOT / "Domain/measurement/unit_converter.c"),
                str(ROOT / "Domain/measurement/mass_math.c"),
                str(ROOT / "Tests/host/stage5p_display_replay.c"),
                "-o", str(output)]
    subprocess.run(command, check=True)
    return command

def replay(runner, rows, output):
    request = "".join(
        f"{r['sequence']} {r['uptime_ms']} "
        f"{r['net_ug'] if r['weight_view'] == '0' else r['gross_ug']} "
        f"{r['stable']} {r['weight_view']}\n" for r in rows)
    response = subprocess.run([str(runner)], input=request, text=True,
                              capture_output=True, check=True).stdout
    values = [tuple(map(int, line.split(","))) for line in response.splitlines()]
    if len(values) != len(rows):
        raise ValueError("runner output row count mismatch")
    with output.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["sequence", "uptime_ms", "display_count",
                         "state", "locked", "reason"])
        writer.writerows((r["sequence"], r["uptime_ms"], *v)
                         for r, v in zip(rows, values))
    counts = [v[0] for v in values]
    return dict(changes=sum(a != b for a, b in zip(counts, counts[1:])),
                count_min=min(counts), count_max=max(counts),
                output_sha256=hashlib.sha256(output.read_bytes()).hexdigest())

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("captures", type=Path, nargs="+")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--cc", default="cc")
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    runners = {name: a.output.resolve() / name for name in ("baseline", "candidate")}
    commands = {name: build(a.cc, runner, name == "baseline")
                for name, runner in runners.items()}
    reports = []
    for index, path in enumerate(a.captures):
        with path.open(newline="") as f:
            rows = list(csv.DictReader(f))
        if not rows:
            raise ValueError("empty capture")
        sequence = [int(r["sequence"]) for r in rows]
        elapsed = (int(rows[-1]["uptime_ms"]) - int(rows[0]["uptime_ms"])) & 0xffffffff
        gaps = [((b - x) & 0xffffffff) for x, b in zip(sequence, sequence[1:])]
        reports.append(dict(
            capture=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            classification="DECIMATED_NON_ATOMIC_HOST_SNAPSHOT_REPLAY",
            rows=len(rows), elapsed_ms=elapsed,
            observed_rows_per_second=(len(rows)-1)*1000/elapsed if elapsed else None,
            duplicate_sequences=sum(x == 0 for x in gaps),
            skipped_conversions=sum(max(0, x-1) for x in gaps),
            recorded_changes=sum(x["display_count"] != y["display_count"]
                                 for x, y in zip(rows, rows[1:])),
            results={name: replay(runner, rows, a.output / f"{index}_{name}.csv")
                     for name, runner in runners.items()}))
    report = dict(baseline_commit=BASELINE, config=dict(unit="g", decimals=2,
                  division=1), compiler_commands=commands, captures=reports,
                  limitation="No interpolation or inference of missing 40 Hz conversions; "
                             "host fields were read in separate Modbus requests; "
                             "software counts only, not LED/panel qualification.")
    (a.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(reports, indent=2))

if __name__ == "__main__":
    main()
