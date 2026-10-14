"""Hash a Stage 5P evidence tree without editing its source artifacts."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2] /
                        "Results/stage5p_integrated")
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = root / "manifest.json"
    files = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path == manifest:
            continue
        data = path.read_bytes()
        files.append({"path": path.relative_to(root).as_posix(),
                      "bytes": len(data),
                      "sha256": hashlib.sha256(data).hexdigest().upper()})
    manifest.write_text(json.dumps({"files": files}, indent=2) + "\n",
                        encoding="utf-8")
    print(f"hashed {len(files)} artifacts")


if __name__ == "__main__":
    main()
