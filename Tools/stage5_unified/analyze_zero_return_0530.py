"""Offline audit of frozen 0x0530 recordings; never opens a device.

Recover the complete R5 FC03 block from raw frames instead of adding weights
and offsets from different requests. All times remain host observations.
"""
import argparse
import csv
import gzip
import hashlib
import io
import json
import math
import statistics
from pathlib import Path

EVIDENCE_COMMIT = "5913167f904a8783b3547ea33ab0d471bd80789d"
EVIDENCE_MANIFEST_SHA256 = "8609292F336166EBE594185DE22BFADFCB0E0410D01F0D2197114454521B264A"
FIRMWARE_COMMIT = "a2174ede65a1927ae491e73289c95bdcaa97deb5"
REQUESTS = ((0, 32), (0x20, 28), (0x1E0, 17), (0x280, 40))
THRESHOLD_UG = 250_000_000
GRAM_UG = 1_000_000
CSV_FIELDS = ("utc", "host_monotonic_ns", "sequence", "uptime_ms", "raw_adc",
    "filtered_raw", "net_ug", "gross_ug", "display_state", "display_locked",
    "display_mass_ug", "display_anchor_ug", "display_threshold_ug",
    "candidate_elapsed_ms", "release_reason", "r5_application", "r5_mode",
    "r5_state", "r5_offset_ug", "stable", "fault", "overrun", "display_count", "weight_view")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def crc16(data):
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def integer(words, start, size, signed=True):
    raw = b"".join(word.to_bytes(2, "big") for word in words[start:start + size])
    if len(raw) != 2 * size:
        raise ValueError("short word field")
    return int.from_bytes(raw, "big", signed=signed)


def exchanges(path):
    pending = None
    with path.open(encoding="ascii") as stream:
        for line_number, line in enumerate(stream, 1):
            utc, stamp, direction, hex_bytes = line.strip().split(maxsplit=3)
            packet = bytes.fromhex(hex_bytes)
            if len(packet) < 5 or crc16(packet[:-2]) != int.from_bytes(packet[-2:], "little"):
                raise ValueError(f"{path}:{line_number}: CRC/short frame")
            stamp = int(stamp)
            if direction == "TX":
                if pending is not None or len(packet) != 8 or packet[:2] != b"\x01\x03":
                    raise ValueError(f"{path}:{line_number}: unpaired/non-FC03 request")
                address = int.from_bytes(packet[2:4], "big")
                count = int.from_bytes(packet[4:6], "big")
                if not 1 <= count <= 125:
                    raise ValueError("invalid FC03 count")
                pending = (address, count, stamp, utc)
            elif direction == "RX":
                if pending is None:
                    raise ValueError("response without request")
                address, count, before_ns, before_utc = pending
                if (packet[:3] != bytes((1, 3, count * 2)) or
                        len(packet) != 5 + count * 2 or stamp < before_ns):
                    raise ValueError(f"{path}:{line_number}: response mismatch")
                words = [int.from_bytes(packet[i:i + 2], "big")
                         for i in range(3, 3 + count * 2, 2)]
                yield dict(address=address, count=count, words=words,
                           tx_ns=before_ns, rx_ns=stamp,
                           tx_utc=before_utc, rx_utc=utc)
                pending = None
            else:
                raise ValueError("unknown frame direction")
    if pending is not None:
        raise ValueError("trailing request without response")


def decode_batch(batch):
    if tuple((x["address"], x["count"]) for x in batch) != REQUESTS:
        raise ValueError("recording request order/count changed")
    rt, dg, dp, r5 = [item["words"] for item in batch]
    row = dict(
        sequence=integer(dg, 0, 2, False), uptime_ms=integer(dg, 2, 2, False),
        raw_adc=integer(rt, 28, 2), filtered_raw=integer(rt, 30, 2),
        net_ug=integer(rt, 16, 4), gross_ug=integer(rt, 20, 4),
        display_state=dp[0], display_locked=dp[1],
        display_mass_ug=integer(dp, 2, 4), display_anchor_ug=integer(dp, 6, 4),
        display_threshold_ug=integer(dp, 10, 4),
        candidate_elapsed_ms=integer(dp, 14, 2, False), release_reason=dp[16],
        r5_application=r5[1], r5_mode=r5[2], r5_state=r5[3],
        r5_offset_ug=integer(r5, 6, 4), stable=int(bool(rt[4] & 0x10)),
        fault=integer(dg, 25, 2, False), overrun=integer(dg, 13, 2, False),
        display_count=integer(rt, 0, 2), weight_view=rt[13])
    if (rt[14], rt[2], rt[3], r5[0]) != (0x010F, 2, 1, 0x55B5):
        raise ValueError("map, decimal places, gram unit or R5 signature changed")
    row.update(
        firmware=rt[15], tare_ug=integer(rt, 24, 4), r5_limited=r5[4],
        r5_uncompensated_ug=integer(r5, 10, 4),
        r5_corrected_ug=integer(r5, 14, 4), r5_reference_ug=integer(r5, 18, 4),
        r5_reference_fill=r5[33], r5_observation_fill=r5[34],
        r5_rebuild_count=integer(r5, 35, 2, False), r5_save_count=r5[39],
        r5_rx_ns=batch[3]["rx_ns"], primary_rx_ns=batch[0]["rx_ns"],
        request_span_ms=(batch[3]["rx_ns"] - batch[0]["tx_ns"]) / 1e6)
    row["r5_equation_error_ug"] = (row["r5_corrected_ug"] -
        row["r5_uncompensated_ug"] + row["r5_offset_ug"])
    # Same primary FC03 request, but the conditioner deliberately retains history.
    row["display_minus_net_ug"] = row["display_count"] * 10_000 - row["net_ug"]
    return row


def load_run(directory, firmware):
    raw = iter(exchanges(directory / "frames.txt"))
    result = []
    with (directory / "samples.csv").open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != list(CSV_FIELDS):
            raise ValueError("CSV schema changed")
        for number, item in enumerate(reader, 1):
            try:
                decoded = decode_batch([next(raw) for _ in REQUESTS])
            except StopIteration as exc:
                raise ValueError(f"raw frames shorter than CSV at row {number}") from exc
            for key, value in item.items():
                if key not in ("utc", "host_monotonic_ns") and decoded[key] != int(value):
                    raise ValueError(f"raw/CSV mismatch row {number}: {key}")
            if decoded["firmware"] != firmware:
                raise ValueError("firmware identity changed")
            decoded.update(utc=item["utc"], host_monotonic_ns=int(item["host_monotonic_ns"]))
            result.append(decoded)
    if next(raw, None) is not None:
        raise ValueError("raw frames longer than CSV")
    if not result:
        raise ValueError("empty recording")
    summary = json.loads((directory / "summary.json").read_text())
    if (summary["rows"] != len(result) or summary["read_errors"] or
            summary["samples_sha256"] != digest(directory / "samples.csv")):
        raise ValueError("run summary mismatch/errors")
    return result


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return None
    def percentile(fraction):
        position = (len(ordered) - 1) * fraction
        low, high = math.floor(position), math.ceil(position)
        return ordered[low] + (ordered[high] - ordered[low]) * (position - low)
    return dict(median=statistics.median(ordered), mean=statistics.fmean(ordered),
                min=ordered[0], max=ordered[-1], p05=percentile(.05), p95=percentile(.95))


STAT_FIELDS = ("raw_adc", "filtered_raw", "net_ug", "display_count",
               "display_minus_net_ug", "r5_uncompensated_ug", "r5_corrected_ug",
               "r5_offset_ug", "r5_reference_ug")


def describe(rows):
    return dict(rows=len(rows), first_utc=rows[0]["utc"], last_utc=rows[-1]["utc"],
                stable_fraction=statistics.fmean(row["stable"] for row in rows),
                r5_states=sorted({row["r5_state"] for row in rows}),
                display_states=sorted({row["display_state"] for row in rows}),
                stats={field: distribution([row[field] for row in rows]) for field in STAT_FIELDS})


def window(rows, origin_ns, begin_s, end_s, loaded):
    start, end = origin_ns + round(begin_s * 1e9), origin_ns + round(end_s * 1e9)
    selected = [row for row in rows if start <= row["host_monotonic_ns"] < end]
    result = dict(relative_interval_s=[begin_s, end_s], rows=len(selected), complete=False)
    # At most one 250 ms poll at either edge and no hidden long interruption.
    if (not selected or selected[0]["host_monotonic_ns"] - start > 250_000_000 or
            end - selected[-1]["host_monotonic_ns"] > 250_000_000 or
            any(b["host_monotonic_ns"] - a["host_monotonic_ns"] > 250_000_000
                for a, b in zip(selected, selected[1:]))):
        result["status"] = "INCOMPLETE_WINDOW"
        return result
    if any((row["net_ug"] >= THRESHOLD_UG) != loaded for row in selected):
        result["status"] = "MIXED_LOAD_STATES"
        return result
    result.update(describe(selected), complete=True, status="OBSERVED_WINDOW")
    return result


def compare(before, after):
    if not before["complete"] or not after["complete"]:
        return dict(status="INCOMPLETE_WINDOW")
    changes = {}
    for field in STAT_FIELDS:
        changes[field] = {kind: after["stats"][field][kind] - before["stats"][field][kind]
                          for kind in ("median", "mean")}
    equation_error = (changes["r5_corrected_ug"]["mean"] -
                      changes["r5_uncompensated_ug"]["mean"] +
                      changes["r5_offset_ug"]["mean"])
    if abs(equation_error) > 1e-6:
        raise ValueError("same-block mean decomposition failed")
    return dict(status="OBSERVED_DIFFERENCE_NOT_METROLOGY_PASS", delta=changes,
                r5_mean_decomposition_error_ug=equation_error)


def edge_analysis(rows):
    events = []
    for index in range(1, len(rows)):
        before, after = rows[index - 1:index + 1]
        loaded = after["net_ug"] >= THRESHOLD_UG
        if loaded == (before["net_ug"] >= THRESHOLD_UG):
            continue
        origin = after["host_monotonic_ns"]
        events.append(dict(direction="LOAD" if loaded else "UNLOAD", row_index=index,
            bracket_utc=[before["utc"], after["utc"]], origin_ns=origin,
            pre=window(rows, origin, -45, -15, not loaded),
            post_15_45=window(rows, origin, 15, 45, loaded),
            post_60_90=window(rows, origin, 60, 90, loaded),
            post_90_120=window(rows, origin, 90, 120, loaded)))
    cycles = []
    for index, load in enumerate(events):
        if load["direction"] != "LOAD" or index + 1 == len(events):
            continue
        unload = events[index + 1]
        if unload["direction"] != "UNLOAD":
            continue
        cycles.append(dict(load_edge=index + 1, unload_edge=index + 2,
            return_15_45=compare(load["pre"], unload["post_15_45"]),
            return_60_90=compare(load["pre"], unload["post_60_90"]),
            return_90_120=compare(load["pre"], unload["post_90_120"]),
            same_post_unload_age_15_45=compare(events[index - 1]["post_15_45"],
                unload["post_15_45"]) if index > 0 else None))
    for event in events:
        event["step_15_45"] = compare(event["pre"], event["post_15_45"])
    return dict(events=events, complete_empty_load_empty_cycles=cycles,
                threshold_ug=THRESHOLD_UG,
                boundary="Starts loaded and ends loaded; first UNLOAD and last LOAD do not form additional empty/load/empty cycles.")


def quality(rows):
    gaps = [b["host_monotonic_ns"] - a["host_monotonic_ns"] for a, b in zip(rows, rows[1:])]
    def deltas(field):
        return [(b[field] - a[field]) & 0xFFFFFFFF for a, b in zip(rows, rows[1:])]
    seq = deltas("sequence")
    ticks = deltas("uptime_ms")
    return dict(rows=len(rows), duration_s=(rows[-1]["host_monotonic_ns"] -
        rows[0]["host_monotonic_ns"]) / 1e9, frames_verified=len(rows) * 8,
        csv_fields_checked_per_row=22, max_host_gap_ms=max(gaps, default=0) / 1e6,
        sequence_duplicates=sum(x == 0 for x in seq),
        sequence_regressions=sum(x > 0x80000000 for x in seq),
        uptime_regressions=sum(x > 0x80000000 for x in ticks),
        omitted_conversion_estimate=sum(max(0, x - 1) for x in seq if x < 0x80000000),
        faults=sum(row["fault"] != 0 for row in rows),
        overruns=sum(row["overrun"] != 0 for row in rows),
        r5_equation_mismatches=sum(row["r5_equation_error_ug"] != 0 for row in rows),
        r5_application_mode_limited=sorted({(row["r5_application"], row["r5_mode"],
            row["r5_limited"]) for row in rows}),
        tare_values=sorted({row["tare_ug"] for row in rows}),
        save_counts=sorted({row["r5_save_count"] for row in rows}),
        same_block_r5_arithmetic=True, cross_request_snapshot_atomic=False)


def audit(evidence, output):
    if digest(evidence / "manifest.json") != EVIDENCE_MANIFEST_SHA256:
        raise ValueError("input is not the frozen hardware evidence manifest")
    manifest = json.loads((evidence / "manifest.json").read_text())
    for item in manifest["files"]:
        path = evidence / item["path"]
        if path.stat().st_size != item["bytes"] or digest(path) != item["sha256"]:
            raise ValueError("evidence manifest mismatch: " + item["path"])
    result = dict(evidence_commit=EVIDENCE_COMMIT, firmware_commit=FIRMWARE_COMMIT,
        hardware_manifest_files_verified=len(manifest["files"]), runs={},
        hardware_manifest_sha256=EVIDENCE_MANIFEST_SHA256,
        classification="OFFLINE_OBSERVATIONS_HARDWARE_CAUSALITY_PENDING",
        limits=["Not a replay of omitted 40 Hz conversions.",
                "R5-block arithmetic is coherent; CSV requests are not a single snapshot.",
                "Differences of means decompose exactly; differences of medians need not.",
                "No calibrated weight certificate, temperature or R5-OFF control is available."])
    output.mkdir(parents=True, exist_ok=False)
    for name in ("baseline_static", "candidate_static_120s", "candidate_static_600s", "three_round_edges"):
        directory = evidence / name
        rows = load_run(directory, 0x052E if name == "baseline_static" else 0x0530)
        q = quality(rows)
        if (q["r5_application_mode_limited"] != [(1, 2, 0)] or
                q["tare_values"] != [0] or q["faults"] or q["overruns"] or
                q["sequence_regressions"] or q["uptime_regressions"] or
                q["r5_equation_mismatches"]):
            raise ValueError("unexpected state/arithmetic in " + name)
        item = dict(quality=q, stats=describe(rows),
                    source_csv_sha256=digest(directory / "samples.csv"),
                    source_frames_sha256=digest(directory / "frames.txt"))
        if name == "three_round_edges":
            item["edges"] = edge_analysis(rows)
        result["runs"][name] = item
        fields = ("utc", "host_monotonic_ns", "r5_rx_ns", "r5_uncompensated_ug",
                  "r5_corrected_ug", "r5_offset_ug", "r5_reference_ug", "r5_state",
                  "r5_reference_fill", "r5_observation_fill", "r5_rebuild_count")
        with gzip.GzipFile(filename=str(output / (name + "_recovered_r5.csv.gz")), mode="wb", mtime=0) as compressed, \
                io.TextIOWrapper(compressed, newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fields, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)
    (output / "analysis.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.evidence, args.output)
    print(json.dumps({name: run["quality"] for name, run in result["runs"].items()}, indent=2))


if __name__ == "__main__":
    main()
