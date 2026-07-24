"""Structured report directory and result helpers."""

import csv
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


class ReportWriter:
    def __init__(self, root, interface="unknown", enabled=True):
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.directory = Path(root) / (stamp + "_" + interface)
        self.enabled = enabled
        self.results = []
        self.latencies = []
        if enabled:
            self.directory.mkdir(parents=True, exist_ok=False)
            self.raw_path = self.directory / "raw_frames.log"
            self.raw_path.touch()

    def raw(self, line):
        if self.enabled:
            with self.raw_path.open("a", encoding="utf-8") as stream:
                stream.write(datetime.now(timezone.utc).isoformat() + " " + line + "\n")

    def add(self, name, status, detail="", data=None):
        item = {"name": name, "status": status, "detail": detail}
        if data is not None:
            item["data"] = data
        self.results.append(item)
        return status == "PASS"

    def latency(self, row):
        self.latencies.append(dict(row))

    def write_json(self, name, value):
        if self.enabled:
            (self.directory / name).write_text(json.dumps(value, indent=2,
                ensure_ascii=False, default=str), encoding="utf-8")

    def finish(self):
        if not self.enabled:
            return None
        self.write_json("results.json", self.results)
        environment_path = self.directory / "environment.json"
        if environment_path.exists():
            environment = json.loads(environment_path.read_text(encoding="utf-8"))
            environment["end_timestamp_utc"] = datetime.now(timezone.utc).isoformat()
            self.write_json("environment.json", environment)
        keys = sorted({key for row in self.latencies for key in row}) or ["request", "complete_ms"]
        with (self.directory / "latency.csv").open("w", newline="",
                encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, keys)
            writer.writeheader()
            writer.writerows(self.latencies)
        lines = ["# Stage 5B-H Summary", ""]
        for result in self.results:
            lines.append("- **%s** %s: %s" %
                         (result["status"], result["name"], result["detail"]))
        lines.extend(["", "Microsecond timing: NOT VERIFIED WITH CURRENT EQUIPMENT",
                      "PVD threshold: REQUIRES ADJUSTABLE POWER SUPPLY"])
        (self.directory / "summary.md").write_text("\n".join(lines) + "\n",
                                                    encoding="utf-8")
        return self.directory


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def git_identity(repo_root):
    def run(*args):
        return subprocess.check_output(["git", *args], cwd=str(repo_root),
            text=True, stderr=subprocess.STDOUT).strip()
    return {"sha": run("rev-parse", "HEAD"),
            "status": run("status", "--short")}


def environment_info(port_info=None):
    try:
        import serial
        serial_version = serial.VERSION
    except ImportError:
        serial_version = None
    return {"os": platform.platform(), "python": sys.version,
            "pyserial": serial_version, "port": port_info,
            "timestamp_utc": datetime.now(timezone.utc).isoformat(),
            "cwd_redacted": os.path.basename(os.getcwd())}
