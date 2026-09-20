#!/usr/bin/env python3
"""Build or verify the portable D1-C evidence manifest."""

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5e_d1b"))
import evidence_manifest as common


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("build")
    build.add_argument("--input", required=True, type=Path)
    build.add_argument("--git-revision", required=True)
    build.add_argument("--classification",
        default="STAGE5MR5E_D1C_DIRECTIONAL_DISPLAY_SOFTWARE_FREEZE")
    verify = sub.add_parser("verify")
    verify.add_argument("--input", required=True, type=Path)
    args = parser.parse_args(); evidence = args.input.resolve()
    if args.command == "build":
        value = common.build(evidence, args.git_revision, ROOT)
        value["classification"] = args.classification
        (evidence / "run_manifest_v2.json").write_text(
            json.dumps(value, indent=2) + "\n", encoding="utf-8")
        print("BUILT %d files" % len(value["files"]))
        return 0
    errors = common.verify(evidence, ROOT)
    if errors:
        print("\n".join(errors))
        return 2
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
