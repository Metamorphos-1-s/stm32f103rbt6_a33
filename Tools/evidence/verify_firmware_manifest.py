"""Verify a firmware evidence manifest against repository bytes and fields."""

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


SHA256_PATTERN = re.compile(r"^[0-9A-F]{64}$")
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def _load_json(path, errors):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        errors.append("%s is not valid UTF-8 JSON: %s" % (path, exc))
        return None


def _repository_path(root, relative, label, errors):
    if not isinstance(relative, str) or not relative:
        errors.append("%s path is missing" % label)
        return None
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError:
        errors.append("%s escapes repository root: %s" % (label, relative))
        return None
    if not candidate.is_file():
        errors.append("%s does not exist: %s" % (label, relative))
        return None
    return candidate


def _check_artifact(root, record, path_key, hash_key, length_key, label, errors):
    path = _repository_path(root, record.get(path_key), label, errors)
    expected_hash = record.get(hash_key)
    expected_length = record.get(length_key)
    if not isinstance(expected_hash, str) or not SHA256_PATTERN.fullmatch(expected_hash):
        errors.append("%s %s must be an uppercase SHA-256" % (label, hash_key))
    if not isinstance(expected_length, int) or expected_length < 0:
        errors.append("%s %s must be a non-negative integer" % (label, length_key))
    if path is None:
        return None
    actual_length = path.stat().st_size
    actual_hash = _sha256(path)
    if actual_length != expected_length:
        errors.append("%s length mismatch: expected %s, got %s" %
                      (label, expected_length, actual_length))
    if actual_hash != expected_hash:
        errors.append("%s SHA-256 mismatch: expected %s, got %s" %
                      (label, expected_hash, actual_hash))
    return path


def _expect(actual, expected, label, errors):
    if actual != expected:
        errors.append("%s mismatch: expected %r, got %r" %
                      (label, expected, actual))


def verify_manifest(root, manifest_path):
    root = Path(root).resolve()
    manifest_path = Path(manifest_path).resolve()
    errors = []
    manifest = _load_json(manifest_path, errors)
    if manifest is None:
        return errors

    _expect(manifest.get("manifest_schema"), 1, "manifest_schema", errors)
    firmware = manifest.get("firmware")
    register_map = manifest.get("register_map")
    schema = manifest.get("schema")
    if not isinstance(firmware, str) or not re.fullmatch(r"0x[0-9A-F]{4}", firmware):
        errors.append("firmware must use 0xNNNN uppercase form")
    if not isinstance(register_map, str) or not re.fullmatch(r"0x[0-9A-F]{4}", register_map):
        errors.append("register_map must use 0xNNNN uppercase form")
    if not isinstance(schema, int) or schema <= 0:
        errors.append("schema must be a positive integer")
    for key in ("production_commit", "evidence_commit"):
        value = manifest.get(key)
        if not isinstance(value, str) or not COMMIT_PATTERN.fullmatch(value):
            errors.append("%s must be a full lowercase Git commit" % key)
    release_hash = manifest.get("release_elf_sha256")
    if not isinstance(release_hash, str) or not SHA256_PATTERN.fullmatch(release_hash):
        errors.append("release_elf_sha256 must be an uppercase SHA-256")

    final_probe = manifest.get("final_probe")
    final_region = manifest.get("final_config_region")
    if not isinstance(final_probe, dict):
        errors.append("final_probe must be an object")
        return errors
    if not isinstance(final_region, dict):
        errors.append("final_config_region must be an object")
        return errors

    probe_path = _check_artifact(root, final_probe, "path", "sha256", "length",
                                 "final probe", errors)
    region_path = _check_artifact(root, final_region, "path", "sha256", "length",
                                  "final config region", errors)
    parsed_path = _check_artifact(root, final_region, "parsed_path",
                                  "parsed_sha256", "parsed_length",
                                  "parsed config region", errors)

    probe = _load_json(probe_path, errors) if probe_path else None
    parsed = _load_json(parsed_path, errors) if parsed_path else None
    if isinstance(probe, list) and len(probe) == 1 and isinstance(probe[0], dict):
        entry = probe[0]
        data = entry.get("data", {})
        _expect(entry.get("status"), "PASS", "final probe status", errors)
        if isinstance(firmware, str) and firmware.startswith("0x"):
            _expect(data.get("firmware_version"), int(firmware, 16),
                    "final probe firmware", errors)
        if isinstance(register_map, str) and register_map.startswith("0x"):
            _expect(data.get("register_map_version"), int(register_map, 16),
                    "final probe register map", errors)
        _expect(data.get("schema_version"), schema, "final probe schema", errors)
        _expect(data.get("config_dirty"), final_probe.get("dirty"),
                "final probe dirty", errors)
        slot_number = {"A": 1, "B": 2}.get(final_probe.get("active_slot"))
        _expect(data.get("active_slot"), slot_number, "final probe active slot", errors)
        _expect(data.get("storage_sequence"), final_probe.get("sequence"),
                "final probe sequence", errors)
    elif probe is not None:
        errors.append("final probe must contain exactly one result object")

    if isinstance(parsed, dict):
        active_slot = final_probe.get("active_slot")
        _expect(parsed.get("source"), region_path.name if region_path else None,
                "parsed source", errors)
        _expect(parsed.get("active_slot"), active_slot, "parsed active slot", errors)
        comparison = parsed.get("modbus_comparison", {})
        _expect(comparison.get("active_slot_matches"), True,
                "parsed active-slot comparison", errors)
        _expect(comparison.get("sequence_matches"), True,
                "parsed sequence comparison", errors)
        active = next((slot for slot in parsed.get("slots", [])
                       if slot.get("slot") == active_slot), None)
        if active is None:
            errors.append("parsed config region has no active slot record")
        else:
            _expect(active.get("valid"), True, "active slot validity", errors)
            _expect(active.get("payload_schema_version"), schema,
                    "active slot schema", errors)
            _expect(active.get("sequence"), final_probe.get("sequence"),
                    "active slot sequence", errors)
            brightness = active.get("persistent", {}).get("display", {}).get("brightness")
            _expect(brightness, final_probe.get("brightness"),
                    "active slot brightness", errors)
    elif parsed is not None:
        errors.append("parsed config region must be an object")

    _expect(final_probe.get("revision"), final_probe.get("saved_revision"),
            "final saved revision", errors)
    _expect(final_probe.get("saved_revision"), final_probe.get("sequence"),
            "final persisted sequence", errors)
    if region_path is not None and region_path.stat().st_size != 4096:
        errors.append("final config region must contain two 2048-byte slots")
    return errors


def main(argv=None):
    script = Path(__file__).resolve()
    default_root = script.parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=default_root)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args(argv)
    root = args.root.resolve()
    manifest = args.manifest or (
        root / "Docs/evidence/FIRMWARE_050F_UNIFIED_MENU_MANIFEST.json")
    errors = verify_manifest(root, manifest)
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    print("PASS: %s" % manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
