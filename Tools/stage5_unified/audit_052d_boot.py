"""Audit archived 0x052D programming evidence without accessing hardware.

The audit intentionally does not infer that a verified Flash write booted the
MCU. In particular, the 0x052D archive has no full post-write application
readback, explicit reset, or CPU-state capture before rollback.
"""

import argparse
import hashlib
import json
import struct
from pathlib import Path


def sha256(data):
    return hashlib.sha256(data).hexdigest().upper()


def audit(root, postflash_application=None):
    software = root / "software" / "firmware_0x052D.bin"
    hardware = root / "hardware"
    candidate = software.read_bytes()
    prefix = (hardware / "postflash_prefix.bin").read_bytes()
    config_before = (hardware / "config_before.bin").read_bytes()
    config_after = (hardware / "config_after_flash.bin").read_bytes()
    erase_log = (hardware / "application_erase.log").read_text(errors="replace")
    write_log = (hardware / "program_verify.log").read_text(errors="replace")
    baseline_write_log = (root.parents[0] / "stage5p_052c" / "hardware" /
                          "application_program_verify.log")
    baseline_log = baseline_write_log.read_text(errors="replace")

    initial_sp, reset_vector = struct.unpack_from("<II", candidate)
    checks = {
        "candidate_sha256": sha256(candidate),
        "candidate_bytes": len(candidate),
        "candidate_vector_sp_in_ram": 0x20000000 <= initial_sp <= 0x20005000,
        "candidate_reset_vector_in_image": (
            reset_vector & 1 == 1 and
            0x08000000 <= (reset_vector & ~1) < 0x08000000 + len(candidate)),
        "postflash_vector_prefix_matches_candidate": (
            len(prefix) >= 256 and prefix == candidate[:len(prefix)]),
        "config_unchanged_after_candidate_write": config_before == config_after,
        "explicit_erase_succeeded": (
            "Existing specified sectors are erased successfully" in erase_log and
            "Error:" not in erase_log),
        "program_verify_succeeded": "Download verified successfully" in write_log,
        "program_log_confirms_mcu_reset": (
            "MCU Reset" in write_log and "Software reset is performed" in write_log),
        "previous_052c_program_log_confirms_mcu_reset": (
            "MCU Reset" in baseline_log and
            "Software reset is performed" in baseline_log),
        "full_postflash_application_readback_available": postflash_application is not None,
    }
    if postflash_application is not None:
        data = postflash_application.read_bytes()
        checks["full_postflash_application_matches_candidate_and_erased_tail"] = (
            len(data) == 126976 and data[:len(candidate)] == candidate and
            all(value == 0xFF for value in data[len(candidate):]))
    checks["programming_preconditions_met"] = all((
        checks["candidate_vector_sp_in_ram"],
        checks["candidate_reset_vector_in_image"],
        checks["postflash_vector_prefix_matches_candidate"],
        checks["config_unchanged_after_candidate_write"],
        checks["explicit_erase_succeeded"],
        checks["program_verify_succeeded"],
        checks["program_log_confirms_mcu_reset"],
        checks.get("full_postflash_application_matches_candidate_and_erased_tail", False),
    ))
    # Even all of these programming checks cannot prove that application
    # startup and Modbus work. The hardware readback remains a separate gate.
    checks["boot_proven"] = False
    checks["modbus_boot_observed"] = False
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2] /
                        "Results" / "stage5p_052d_display_candidate")
    parser.add_argument("--postflash-application", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = audit(args.root, args.postflash_application)
    serialized = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized, encoding="utf-8")
    else:
        print(serialized, end="")


if __name__ == "__main__":
    main()
