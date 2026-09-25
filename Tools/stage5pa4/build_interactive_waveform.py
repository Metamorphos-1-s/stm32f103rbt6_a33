#!/usr/bin/env python3
"""Build a standalone, offline, zoomable waveform from Stage 5P-A4 CSVs."""

import argparse
import csv
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = Path(__file__).with_name("interactive_waveform_template.html")
DATA_FIELDS = ("gross_ug", "display_count", "conditioned_display_ug",
               "raw_adc", "filtered_raw")


def utc_ms(text):
    date = datetime.fromisoformat(text.replace("Z", "+00:00"))
    if date.tzinfo is None:
        raise ValueError("UTC time must include an offset")
    return round(date.timestamp() * 1000)


def optional_number(row, field, divisor=1):
    raw = row.get(field)
    if raw in (None, ""):
        return None
    return int(raw) / divisor


def read_samples(path):
    samples = []
    with path.open(encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        if not {"utc", "host_monotonic_ns", "gross_ug"}.issubset(reader.fieldnames or ()):
            raise ValueError("samples.csv lacks UTC, monotonic time or gross_ug")
        for row in reader:
            monotonic = int(row["host_monotonic_ns"])
            decimals = row.get("display_decimals")
            display = (optional_number(row, "display_count", 10 ** int(decimals))
                       if decimals not in (None, "") else None)
            item = [monotonic, optional_number(row, "gross_ug", 1e6),
                    display,
                    optional_number(row, "conditioned_display_ug", 1e6),
                    optional_number(row, "raw_adc"),
                    optional_number(row, "filtered_raw")]
            if item[1] is None:
                raise ValueError("gross_ug cannot be empty")
            if samples and monotonic <= samples[-1][0]:
                raise ValueError("host_monotonic_ns is not strictly increasing")
            samples.append(item)
            if len(samples) == 1:
                first_utc = utc_ms(row["utc"])
                first_monotonic = monotonic
    if not samples:
        raise ValueError("samples.csv contains no records")
    return first_utc, first_monotonic, [
        [round((item[0] - first_monotonic) / 1e6), *item[1:]]
        for item in samples]


def read_events(path, start_utc_ms):
    result = []
    if not path.is_file():
        return result
    with path.open(encoding="utf-8") as source:
        for line in source:
            if not line.strip():
                continue
            event = json.loads(line)
            if event.get("event") in ("UNLOAD_1_OPERATOR_CONFIRMED",
                                      "LOAD_2_OPERATOR_CONFIRMED",
                                      "UNLOAD_2_OPERATOR_CONFIRMED"):
                result.append({"t": utc_ms(event["utc"]) - start_utc_ms,
                               "kind": "operator", "label": event["event"],
                               "utc": event["utc"]})
    return result


def read_edges(path, start_utc_ms):
    edges = []
    if not path.is_file():
        return edges
    with path.open(encoding="utf-8-sig", newline="") as source:
        for row in csv.DictReader(source):
            if not row.get("gross_before_utc") or not row.get("gross_after_utc"):
                continue
            before = utc_ms(row["gross_before_utc"])
            after = utc_ms(row["gross_after_utc"])
            edges.append({"t": round((before + after) / 2) - start_utc_ms,
                          "begin": before - start_utc_ms,
                          "end": after - start_utc_ms,
                          "kind": "edge", "label": row["event"],
                          "utc": row["gross_before_utc"] + " – " + row["gross_after_utc"],
                          "step": row.get("physical_step_ug", "")})
    return edges


def build(input_path, output_path, events_path=None, timeline_path=None):
    first_utc, _, samples = read_samples(input_path)
    events_path = events_path or input_path.with_name("events.jsonl")
    timeline_path = timeline_path or input_path.parents[2] / "event_timeline.csv"
    payload = {"baseUtcMs": first_utc, "points": samples,
               "events": read_events(events_path, first_utc) +
                         read_edges(timeline_path, first_utc),
               "source": str(input_path.resolve()),
               "missing": [field for index, field in enumerate(DATA_FIELDS, 1)
                           if all(item[index] is None for item in samples)],
               "note": "R5 OFF: gross_g is authoritative. ADC traces have units of counts; "
                       "display is quantized. Physical first load was not captured."}
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
    # Inline JSON must not close its own script tag even for arbitrary paths.
    encoded = encoded.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count("__WAVEFORM_DATA__") != 1:
        raise ValueError("HTML template data placeholder is missing or duplicated")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(template.replace("__WAVEFORM_DATA__", encoded), encoding="utf-8")
    return {"points": len(samples), "event_markers": len(payload["events"]),
            "missing_fields": payload["missing"], "output": str(output_path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT /
        "Results/stage5pa4/thermal_cycle/continuous_3h/samples.csv")
    parser.add_argument("--events", type=Path)
    parser.add_argument("--timeline", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT /
        "Results/stage5pa4/interactive_waveform.html")
    args = parser.parse_args()
    print(json.dumps(build(args.input, args.output, args.events, args.timeline),
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
