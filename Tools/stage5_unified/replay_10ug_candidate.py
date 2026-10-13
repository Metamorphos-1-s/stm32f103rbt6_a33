"""Replay the opened A13 traces against the 10 ug/s long-rate candidate."""
import argparse
import csv
import json
import subprocess
import sys
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from check_parity import POLICY, compare, samples, sha  # noqa: E402


def focused(path):
    """Retain consecutive, unique device samples after STATIC first starts."""
    data = []
    modes = []
    previous = None
    with path.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            mode = int(row["mode"])
            if mode == 0:
                continue
            seq = int(row["sample_sequence"])
            if previous is not None and seq == previous:
                continue
            if previous is not None and seq != previous + 1:
                break
            data.append((seq, int(row["mcu_uptime_ms"]),
                         int(row["host_monotonic_ns"]),
                         int(row["gross_ug"])))
            modes.append(mode)
            previous = seq
    intervals = []
    start = None
    for item, mode in zip(data, modes):
        if mode == 1 and start is None:
            start = item[2]
        if mode != 1 and start is not None:
            intervals.append((start, item[2]))
            start = None
    if start is not None:
        intervals.append((start, data[-1][2] + 1))
    return data, tuple(intervals)


def limits(data, intervals, runner):
    payload = ''.join(f'{seq},{ms},{mass},'
                      f'{1 if any(left <= host < right for left, right in intervals) else 2}\n'
                      for seq, ms, host, mass in data)
    run = subprocess.run([str(runner)], input=payload, text=True,
                         capture_output=True, timeout=300, check=True)
    trace = [tuple(map(int, line.split(','))) for line in run.stdout.splitlines()]
    if len(trace) != len(data):
        raise ValueError('C output length differs from input')
    maximum_sample = maximum_step_loss = dosing_changes = 0
    for index in range(1, len(trace)):
        previous, current = trace[index - 1], trace[index]
        change = abs(current[2] - previous[2])
        maximum_sample = max(maximum_sample, change)
        if current[3] == 1:  # A13C_STATE_DOSING
            dosing_changes += change != 0
        if abs(data[index][3] - data[index - 1][3]) >= 2_000_000:
            loss = abs((current[1] - previous[1]) -
                       (data[index][3] - data[index - 1][3]))
            maximum_step_loss = max(maximum_step_loss, loss)
    return {'max_per_sample_offset_ug': maximum_sample,
            'dosing_offset_changes': dosing_changes,
            'max_obvious_step_loss_ug': maximum_step_loss}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    import check_parity
    check_parity.POLICY = replace(POLICY, name="auto_static_350_5m_10ug",
                                  baseline_ug_per_sample=10)
    cases = (
        ("a13b", ROOT / "Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/samples.csv",
         "6D11B9A3D44C6E8DC658EB1F41B99975EC339DD7DBD0D3E61791DCA75B777612"),
        ("a9", ROOT / "Results/stage5pa9/20260927T060806Z_development_capture/samples.csv",
         "E683609BA0DB7EA090D101CE38569A432EC319982E7D41A9D2176A62B623128C"),
        ("a13cr", ROOT / "Results/stage5pa13c_r/static_load_unload_continuous/samples.csv",
         "499D2F14D2E2A04FEE27369209610089AA39D91CD39B1CBABD04A27B8D2CA838"),
    )
    args.output.mkdir(parents=True, exist_ok=True)
    summary = {}
    for name, path, expected_sha in cases:
        if sha(path) != expected_sha:
            raise ValueError(name + " CSV SHA mismatch")
        data, intervals = focused(path) if name == "a13cr" else (list(samples(path)), ())
        if not data:
            raise ValueError(name + " has no usable samples")
        output = args.output / (name + ".json")
        passed = compare(data, intervals, args.runner, output)
        result = json.loads(output.read_text(encoding="utf-8"))
        summary[name] = {"status": "PASS" if passed else "FAIL",
                         "samples": len(data), "source_sha256": expected_sha,
                         "mismatches": result["mismatch_count"],
                         "max_10s_offset_ug": result["c_max_10s_offset_ug"],
                         "first_mismatch": result["first_mismatch"],
                         **limits(data, intervals, args.runner)}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                                   encoding="utf-8")
    return 0 if all(s["status"] == "PASS" for s in summary.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
