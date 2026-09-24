#!/usr/bin/env python3
"""Audit RAM object sizes against the exact 0x051C ARM map.

Object-size subtraction is a projection only. A new ARM map is mandatory for
claiming linked RAM savings or the conservative stack collision margin.
"""

import argparse
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE_COMMIT = "d6312dd9898ba8f01a43cd83a76fa41546d88275"
BASE_MAP = ("Results/stage5pa2d/ram_fix_gate/artifacts/"
            "firmware_0x051C_debug.map")
OLD_SYMBOLS = ("s_slot_a_payload", "s_slot_b_payload",
               "s_factory_config", "s_candidate_target")
NEW_SYMBOLS = ("s_slot_payload", "s_config_transaction")
STACK_BOUND_B = 1504
MIN_COLLISION_B = 512


def pinned_map():
    env = dict(os.environ, GIT_NO_LAZY_FETCH="1")
    return subprocess.check_output(
        ["git", "-C", str(ROOT), "show", f"{BASE_COMMIT}:{BASE_MAP}"],
        env=env, text=True)


def read_symbols(source, names):
    lines = source.splitlines()
    sizes = {}
    for index, line in enumerate(lines):
        match = re.match(r"\s+\.bss\.(\w+)\s*(.*)", line)
        if not match or match.group(1) not in names:
            continue
        rest = match.group(2)
        if not rest.strip() and index + 1 < len(lines):
            rest = lines[index + 1]
        size = re.search(r"0x200[0-9a-f]{5}\s+0x([0-9a-f]+)", rest)
        if size:
            sizes[match.group(1)] = int(size.group(1), 16)
    if set(sizes) != set(names):
        raise ValueError(f"missing RAM symbols: {set(names) - set(sizes)}")
    return sizes


def map_geometry(source):
    end = re.search(r"0x(200[0-9a-f]{5})\s+_ebss =", source)
    top = re.search(r"0x(200[0-9a-f]{5})\s+_estack =", source)
    if not end or not top:
        raise ValueError("missing ARM RAM boundary symbols")
    static_end, ram_end = int(end.group(1), 16), int(top.group(1), 16)
    return {"static_end": hex(static_end), "ram_end": hex(ram_end),
            "physical_stack_room_b": ram_end - static_end,
            "collision_margin_b": ram_end - static_end - STACK_BOUND_B}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate-map", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    baseline = pinned_map()
    sizes = read_symbols(baseline, OLD_SYMBOLS)
    baseline_geometry = map_geometry(baseline)
    # Removing one of the two A/B payloads and combining mutually exclusive
    # factory/transaction configs frees one object of each size, before padding.
    projected_saving = sizes["s_slot_a_payload"] + sizes["s_factory_config"]
    report = {"classification": "SOFTWARE PROJECTION / ARM RETEST REQUIRED",
              "baseline_commit": BASE_COMMIT,
              "baseline_symbols_b": sizes,
              "baseline": baseline_geometry,
              "projected_object_saving_b": projected_saving,
              "projected_collision_margin_b_if_stack_and_padding_unchanged":
                  baseline_geometry["collision_margin_b"] + projected_saving,
              "minimum_collision_margin_b": MIN_COLLISION_B,
              "candidate_target_build": "NOT RUN"}
    if args.candidate_map:
        candidate = args.candidate_map.read_text(encoding="utf-8")
        candidate_geometry = map_geometry(candidate)
        report["candidate_target_build"] = {
            "symbol_sizes_b": read_symbols(candidate, NEW_SYMBOLS),
            "geometry": candidate_geometry,
            "collision_gate_pass":
                candidate_geometry["collision_margin_b"] >= MIN_COLLISION_B,
            "static_end_saving_b":
                int(baseline_geometry["static_end"], 16) -
                int(candidate_geometry["static_end"], 16)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n",
                           encoding="utf-8")
    print(json.dumps({"projected_saving_b": projected_saving,
                      "target_build": report["candidate_target_build"]}))


if __name__ == "__main__":
    main()
