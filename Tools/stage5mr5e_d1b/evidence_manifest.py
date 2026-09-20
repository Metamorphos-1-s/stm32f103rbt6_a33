#!/usr/bin/env python3
"""Build or verify the portable D1-B evidence manifest."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def evidence_files(root):
    return sorted(path for path in root.rglob("*")
        if path.is_file() and path.name != "run_manifest_v2.json")


def git_bytes(repository_root, revision, path):
    return subprocess.check_output(["git", "show",
        "%s:%s" % (revision, path)], cwd=repository_root)


def build(root, repository_commit, repository_root=None):
    if repository_root is None:
        files = [{"path": path.relative_to(root).as_posix(),
            "length": path.stat().st_size, "sha256": digest(path)}
            for path in evidence_files(root)]
    else:
        prefix = root.relative_to(repository_root).as_posix()
        names = subprocess.check_output(["git", "ls-tree", "-r",
            "--name-only", repository_commit, "--", prefix],
            cwd=repository_root, text=True).splitlines()
        files = []
        for name in names:
            relative = Path(name).relative_to(prefix).as_posix()
            if relative == "run_manifest_v2.json":
                continue
            data = git_bytes(repository_root, repository_commit, name)
            files.append({"path": relative, "length": len(data),
                "sha256": hashlib.sha256(data).hexdigest().upper()})
    value = {"schema_version": 2,
        "classification": "STAGE5MR5E_D1B_FAILED_HARDWARE_HOLDOUT",
        "run_id": root.name,
        "repository_commit": repository_commit,
        "files": files}
    target = root / "run_manifest_v2.json"
    target.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    return value


def verify(root, repository_root=None):
    manifest = json.loads((root / "run_manifest_v2.json").read_text(
        encoding="utf-8"))
    errors = []
    for item in manifest["files"]:
        if repository_root is None:
            path = root / item["path"]
            if not path.is_file():
                errors.append("missing: " + item["path"])
                continue
            data = path.read_bytes()
        else:
            path = (root / item["path"]).relative_to(
                repository_root).as_posix()
            try:
                data = git_bytes(repository_root,
                    manifest["repository_commit"], path)
            except subprocess.CalledProcessError:
                errors.append("missing: " + item["path"])
                continue
        if len(data) != item["length"]:
            errors.append("length: " + item["path"])
        elif hashlib.sha256(data).hexdigest().upper() != item["sha256"]:
            errors.append("sha256: " + item["path"])
    return errors


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("build")
    create.add_argument("--input", required=True, type=Path)
    create.add_argument("--git-revision", required=True)
    check = sub.add_parser("verify")
    check.add_argument("--input", required=True, type=Path)
    args = parser.parse_args()
    if args.command == "build":
        repository_root = Path(__file__).resolve().parents[2]
        value = build(args.input.resolve(), args.git_revision,
            repository_root)
        print("BUILT %d files" % len(value["files"]))
        return 0
    repository_root = Path(__file__).resolve().parents[2]
    errors = verify(args.input.resolve(), repository_root)
    if errors:
        print("\n".join(errors))
        return 2
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
