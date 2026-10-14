"""Audit the intentional 0x052B V3 seq22->23 joint-request SAVE."""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5b_hw"))
from parse_config_slots import HEADER_SIZE, SLOT_SIZE, parse_dump  # noqa: E402


def audit(before_path, after_path):
    before = parse_dump(before_path, "B", 22)
    after = parse_dump(after_path, "A", 23)
    prior = before_path.read_bytes()
    saved = after_path.read_bytes()
    old_payload = prior[SLOT_SIZE + HEADER_SIZE:SLOT_SIZE + HEADER_SIZE + 281]
    new_payload = saved[HEADER_SIZE:HEADER_SIZE + 281]
    changed = [i for i, (a, b) in enumerate(zip(old_payload, new_payload))
               if a != b]
    unexpected = [i for i in changed if i not in range(207, 236) and i != 280]
    old_slot_unchanged = prior[SLOT_SIZE:] == saved[SLOT_SIZE:]
    request = after["slots"][0]["persistent"]["requests"]
    alarm = after["slots"][0]["persistent"]["alarm"]
    checks = dict(before_valid=all(slot["valid"] for slot in before["slots"]),
                  after_valid=all(slot["valid"] for slot in after["slots"]),
                  before_active_b=before["active_slot"] == "B",
                  after_active_a=after["active_slot"] == "A",
                  sequences=[slot["sequence"] for slot in after["slots"]] == [23, 22],
                  old_slot_unchanged=old_slot_unchanged,
                  no_unexpected_payload_changes=not unexpected,
                  requested_pair=request == dict(r5_mode=2,
                      r5_application=1, checkweigh_mode=1),
                  alarm_exact=alarm == dict(lower_limit_ug=100000000,
                      upper_limit_ug=400000000, hysteresis_ug=10000,
                      weight_source=0, internal_buzzer_enable=False,
                      external_buzzer_enable=False, qualified_beep_enable=False,
                      limit_function_enable=True))
    return dict(status="PASS" if all(checks.values()) else "FAIL",
                checks=checks, changed_payload_offsets=changed,
                unexpected_offsets=unexpected, before=before, after=after)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.before, args.after)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(status=result["status"], checks=result["checks"],
        changed_payload_offsets=result["changed_payload_offsets"])))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
