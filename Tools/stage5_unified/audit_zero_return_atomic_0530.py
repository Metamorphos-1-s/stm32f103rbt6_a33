"""Offline-only verification of the atomic 0x0530 zero-return evidence.

Validates archived bytes and all six FC03 responses per row. Never imports a
serial transport. Missing manifest entries are reported, never synthesized.
"""
import argparse
import json
import statistics
from pathlib import Path

from analyze_zero_return_0530 import REQUESTS, decode_batch, digest, exchanges, integer

LIMIT_NS = 250_000_000
THRESHOLD_UG = 250_000_000
FIELDS = ("uncompensated_ug", "applied_offset_ug", "gross_ug", "net_ug", "tare_ug")
MANIFEST_SHA256 = "7418C4A420C7C620FA1FD8087D9D0AE8CD08B9247B560EDD2161D1171DCA4A8D"


def decode_authority(words):
    if len(words) != 40 or integer(words, 0, 2, False) != 0xA13E0001:
        raise ValueError("authority signature/count")
    layout = dict(generation=(2, 2, False), application=(8, 2, False),
        sequence=(10, 2, False), uptime_ms=(12, 2, False),
        applied_offset_ug=(14, 2, True), apply=(16, 2, False),
        uncompensated_ug=(18, 4, True), gross_ug=(22, 4, True),
        net_ug=(26, 4, True), tare_ug=(30, 4, True), mode=(34, 2, False),
        state=(36, 2, False), candidate_offset_ug=(38, 2, True))
    result = {key: integer(words, *spec) for key, spec in layout.items()}
    result["snapshot_atomic"] = True
    effective = result["applied_offset_ug"] if result["apply"] else 0
    if (result["apply"] not in (0, 1) or
            result["gross_ug"] != result["uncompensated_ug"] - effective or
            result["net_ug"] != result["gross_ug"] - result["tare_ug"]):
        raise ValueError("authority equation")
    return result


def decode_candidate(words):
    if len(words) != 37 or words[0] != 0xA13C:
        raise ValueError("candidate signature/count")
    result = dict(zip(("mode", "state", "reason", "limited"), words[1:5]))
    names = ("sequence", "uptime_ms", "gate_count", "rebuild_count", "boost_samples",
        "obvious_step_sequence", "robust_step_sequence", "quiet_sequence",
        "reference_lock_sequence", "first_correction_sequence")
    result.update({name: integer(words, 17 + 2 * i, 2, False)
                   for i, name in enumerate(names)})
    return result


def verify_run(directory):
    rows = [json.loads(line) for line in (directory / "samples.jsonl").open()]
    if not rows:
        raise ValueError("empty capture")
    summary = json.loads((directory / "summary.json").read_text())
    if (summary["rows"] != len(rows) or summary["read_errors"] or
            summary["samples_sha256"] != digest(directory / "samples.jsonl") or
            summary["frames_sha256"] != digest(directory / "frames.txt") or
            (directory / "errors.jsonl").stat().st_size):
        raise ValueError("capture summary/hash/errors")
    cursor, total = 0, 0
    frames = iter(exchanges(directory / "frames.txt"))
    for frame in frames:
        total += 1
        if (frame["address"], frame["count"]) != (0x340, 40):
            # Only preflight/postflight probes can occur outside row batches.
            if 0 < cursor < len(rows):
                raise ValueError("unexpected request between row batches")
            continue
        if cursor >= len(rows):
            raise ValueError("more authority responses than JSON rows")
        row = rows[cursor]
        if decode_authority(frame["words"]) != row["authority"]:
            raise ValueError(f"authority/raw mismatch row {cursor + 1}")
        batch = [next(frames) for _ in REQUESTS]
        total += len(batch)
        observation = decode_batch(batch)
        # collect() times the whole read(), while the frame logger times TX/RX
        # inside that call; neither clock span is a payload register.
        payload = set(observation) - {"r5_rx_ns", "primary_rx_ns", "request_span_ms"}
        if (set(observation) != set(row["observation"]) or
                any(observation[k] != row["observation"][k] for k in payload)):
            raise ValueError(f"observation/raw mismatch row {cursor + 1}")
        candidate = next(frames)
        total += 1
        if ((candidate["address"], candidate["count"]) != (0x300, 37) or
                decode_candidate(candidate["words"]) != row["candidate"]):
            raise ValueError(f"candidate/raw mismatch row {cursor + 1}")
        diagnostic = batch[1]["words"]
        if (row["revision"] != integer(diagnostic, 19, 2, False) or
                row["saved_revision"] != integer(diagnostic, 21, 2, False) or
                row["observation_snapshot_atomic"] is not False):
            raise ValueError("revision or atomicity metadata mismatch")
        a, o = row["authority"], row["observation"]
        if ((a["application"], a["mode"], a["apply"], a["tare_ug"]) != (1, 2, 1, 0) or
                (o["firmware"], o["r5_application"], o["r5_mode"], o["r5_limited"],
                 o["fault"], o["overrun"], o["r5_save_count"]) != (0x0530, 1, 2, 0, 0, 0, 0) or
                (row["revision"], row["saved_revision"], diagnostic[18], diagnostic[27]) !=
                (23, 23, 0, 1) or o["tare_ug"] or o["r5_equation_error_ug"] or
                (row["candidate"]["mode"], row["candidate"]["limited"]) != (2, 0)):
            raise ValueError("frozen configuration/safety envelope")
        if cursor and (row["host_monotonic_ns"] <= rows[cursor - 1]["host_monotonic_ns"] or
                a["sequence"] <= rows[cursor - 1]["authority"]["sequence"] or
                a["uptime_ms"] <= rows[cursor - 1]["authority"]["uptime_ms"]):
            raise ValueError("host/device time or sequence did not advance")
        cursor += 1
    if cursor != len(rows):
        raise ValueError("fewer authority responses than JSON rows")
    return rows, dict(rows=cursor, validated_exchanges=total,
        validated_frames=total * 2, first_utc=rows[0]["utc"], last_utc=rows[-1]["utc"],
        max_poll_gap_ms=max((b["host_monotonic_ns"] - a["host_monotonic_ns"]
            for a, b in zip(rows, rows[1:])), default=0) / 1e6)


def find_edges(rows):
    edges = []
    for left, right in zip(rows, rows[1:]):
        if right["host_monotonic_ns"] - left["host_monotonic_ns"] > LIMIT_NS:
            continue  # A gap cannot establish a physical load/unload edge.
        before = left["authority"]["net_ug"] >= THRESHOLD_UG
        after = right["authority"]["net_ug"] >= THRESHOLD_UG
        if before != after:
            edges.append(dict(direction="LOAD" if after else "UNLOAD",
                              before=left, after=right))
    return edges


def window(rows, origin_ns, low, high, loaded=False):
    start, end = origin_ns + round(low * 1e9), origin_ns + round(high * 1e9)
    selected = [r for r in rows if start <= r["host_monotonic_ns"] < end]
    result = dict(interval_s=[low, high], rows=len(selected), complete=False)
    if not selected:
        result["status"] = "NO_OBSERVATIONS"
        return result
    gaps = [b["host_monotonic_ns"] - a["host_monotonic_ns"]
            for a, b in zip(selected, selected[1:])]
    result.update(first_utc=selected[0]["utc"], last_utc=selected[-1]["utc"],
                  max_gap_ms=max(gaps, default=0) / 1e6)
    if (selected[0]["host_monotonic_ns"] - start > LIMIT_NS or
            end - selected[-1]["host_monotonic_ns"] > LIMIT_NS or
            max(gaps, default=0) > LIMIT_NS):
        result["status"] = "INCOMPLETE_WINDOW"
        return result
    if any((r["authority"]["net_ug"] >= THRESHOLD_UG) != loaded for r in selected):
        result["status"] = "MIXED_LOAD_STATES"
        return result
    result.update(complete=True, status="OBSERVED_WINDOW", stats={
        key: dict(mean_ug=statistics.mean(r["authority"][key] for r in selected),
                  median_ug=statistics.median(r["authority"][key] for r in selected),
                  min_ug=min(r["authority"][key] for r in selected),
                  max_ug=max(r["authority"][key] for r in selected)) for key in FIELDS})
    return result


def compare(before, after):
    if not before["complete"] or not after["complete"]:
        return dict(status="INCOMPLETE_COMPARISON")
    delta = {key: after["stats"][key]["mean_ug"] - before["stats"][key]["mean_ug"]
             for key in FIELDS}
    residual = (delta["uncompensated_ug"] - delta["applied_offset_ug"] -
                delta["tare_ug"] - delta["net_ug"])
    if abs(residual) > 1e-6:
        raise ValueError("mean decomposition residual")
    return dict(status="OBSERVED_ARITHMETIC_NOT_OFF_CONTROL", mean_delta_ug=delta,
        equation_residual_ug=residual, net_median_delta_ug=
            after["stats"]["net_ug"]["median_ug"] - before["stats"]["net_ug"]["median_ug"])


def analyze(rows):
    edges = find_edges(rows)
    cycles = []
    for load, unload in zip(edges, edges[1:]):
        if (load["direction"], unload["direction"]) != ("LOAD", "UNLOAD"):
            continue
        begin, end = load["after"]["host_monotonic_ns"], unload["after"]["host_monotonic_ns"]
        before = window(rows, begin, -45, -15)
        post = {f"{a}_{b}": window(rows, end, a, b)
                for a, b in ((15, 45), (60, 90), (90, 120), (120, 150), (270, 300))}
        early = [r for r in rows if 15e9 <= r["host_monotonic_ns"] - end < 45e9]
        events = [r for r in rows if 0 <= r["host_monotonic_ns"] - end < 150e9]
        offset0 = unload["after"]["authority"]["applied_offset_ug"]
        changed = next((r for r in events if r["authority"]["applied_offset_ug"] != offset0), None)
        ref_events = [r for r in events if r["candidate"]["reference_lock_sequence"] and
                      r["candidate"]["reference_lock_sequence"] !=
                      unload["after"]["candidate"]["reference_lock_sequence"]]
        cycles.append(dict(load_interval_utc=[load[k]["utc"] for k in ("before", "after")],
            unload_interval_utc=[unload[k]["utc"] for k in ("before", "after")],
            load_pre=before, loaded=window(rows, begin, 15, 45, loaded=True), post=post,
            loaded_0_120=window(rows, begin, 0, 120, loaded=True),
            unloaded_0_120=window(rows, end, 0, 120),
            comparisons={key: compare(before, value) for key, value in post.items()},
            early_offset_values_ug=sorted({r["authority"]["applied_offset_ug"] for r in early}),
            first_observed_post_unload_offset_change_s=None if changed is None else
                (changed["host_monotonic_ns"] - end) / 1e9,
            first_observed_new_reference_lock_s=None if not ref_events else
                (ref_events[0]["host_monotonic_ns"] - end) / 1e9))
    if not edges:
        raise ValueError("no observed load edges")
    last_unload = next(e for e in reversed(edges) if e["direction"] == "UNLOAD")
    return dict(edges=len(edges), cycles=cycles,
                final_600s=window(rows, last_unload["after"]["host_monotonic_ns"], 0, 600))


def audit(root):
    if digest(root / "manifest.json") != MANIFEST_SHA256:
        raise ValueError("source manifest pin mismatch; this audit is for the fixed October 19 capture")
    manifest = json.loads((root / "manifest.json").read_text())["files"]
    missing, verified = [], []
    for entry in manifest:
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe manifest path")
        path = root / relative
        if not path.exists():
            missing.append(entry["path"])
        elif path.stat().st_size != entry["bytes"] or digest(path) != entry["sha256"]:
            raise ValueError(f"manifest mismatch: {entry['path']}")
        else:
            verified.append(entry["path"])
    rows, main = verify_run(root / "capture")
    tail, extension = verify_run(root / "capture_tail_extension")
    if (tail[0]["host_monotonic_ns"] <= rows[-1]["host_monotonic_ns"] or
            tail[0]["authority"]["sequence"] <= rows[-1]["authority"]["sequence"]):
        raise ValueError("tail is out of order")
    result = analyze(rows + tail)
    result.update(source_manifest_sha256=digest(root / "manifest.json"),
        evidence_commit="f5137c0f1eb6171d4f2e459bd28539f51fb8896f",
        firmware_source_commit="a2174ede65a1927ae491e73289c95bdcaa97deb5",
        verified_manifest_entries=verified, missing_manifest_entries=missing,
        runs=dict(capture=main, capture_tail_extension=extension),
        inter_segment_gap_s=(tail[0]["host_monotonic_ns"] - rows[-1]["host_monotonic_ns"]) / 1e9,
        total_rows=len(rows) + len(tail), equation_errors=0,
        scope="All six FC03 responses matched; only authority fields are used for weight decomposition; no interpolation.")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = audit(args.evidence)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(rows=result["total_rows"], edges=result["edges"],
        cycles=len(result["cycles"]), missing_manifest_entries=result["missing_manifest_entries"],
        final_600s=result["final_600s"]["status"])))


if __name__ == "__main__":
    main()
