"""Hash every archived Stage 5P integrated artifact without editing inputs."""
import hashlib
import json
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[2] / "Results/stage5p_integrated"
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
