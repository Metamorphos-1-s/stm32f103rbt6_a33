"""Clean, software-only Stage 5P display qualification. Never opens a device."""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COMPILER = "GNU Tools for STM32 13.3.rel1.20240926-1715"

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def executable(value):
    found = shutil.which(value) or (str(Path(value).resolve()) if Path(value).is_file() else None)
    if not found:
        raise ValueError("executable not found: " + value)
    return str(Path(found).resolve())

def run(command, log, env=None):
    with log.open("w", encoding="utf-8") as output:
        subprocess.run(list(map(str, command)), cwd=ROOT, env=env, check=True,
                       stdout=output, stderr=subprocess.STDOUT)
    return log.read_text(encoding="utf-8", errors="replace")

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arm-bin", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--cmake", default="cmake")
    p.add_argument("--ninja", default="ninja")
    p.add_argument("--host-cc", default="cc")
    p.add_argument("--jobs", type=int, default=6)
    a = p.parse_args()
    a.output = a.output.resolve()
    if a.output.exists():
        p.error("output must be a new directory")
    arm = a.arm_bin.resolve()
    suffix = ".exe" if os.name == "nt" else ""
    compiler = arm / ("arm-none-eabi-gcc" + suffix)
    identity = subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0]
    if COMPILER not in identity:
        p.error("wrong compiler distribution: " + identity)
    cmake, ninja, host = map(executable, (a.cmake, a.ninja, a.host_cc))
    ctest = Path(cmake).with_name("ctest" + suffix)
    if not ctest.is_file():
        p.error("ctest must be beside cmake")
    a.output.mkdir(parents=True)
    env = dict(os.environ)
    env["PATH"] = str(arm) + os.pathsep + env.get("PATH", "")
    report = {"classification": "SOFTWARE_ONLY_NO_DEVICE_ACCESS",
              "compiler": identity, "host_compiler": subprocess.check_output([host, "--version"], text=True).splitlines()[0],
              "cmake": subprocess.check_output([cmake, "--version"], text=True).splitlines()[0],
              "source_base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
              "commands": [], "host": {}, "arm": {}}
    def logged(command, name, command_env=None):
        report["commands"].append(list(map(str, command)))
        return run(command, a.output / name, command_env)
    for mode in ("Debug", "Release"):
        folder = a.output / ("host_" + mode.lower())
        logged([cmake, "-S", ROOT / "Tests/host", "-B", folder, "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=" + ninja, "-DCMAKE_C_COMPILER=" + host,
                "-DCMAKE_BUILD_TYPE=" + mode, "-DCMAKE_C_FLAGS=-Wall -Wextra -Werror"],
               "host_" + mode.lower() + "_configure.txt")
        logged([cmake, "--build", folder, "-j", a.jobs], "host_" + mode.lower() + "_build.txt")
        text = logged([ctest, "--test-dir", folder, "--output-on-failure"],
                      "host_" + mode.lower() + "_ctest.txt")
        count = re.search(r"100% tests passed out of (\d+)", text)
        if not count:
            raise ValueError("missing complete CTest success summary")
        report["host"][mode] = {"passed": int(count.group(1))}
        print(mode + " full Host PASS: " + count.group(1), flush=True)
    logged([sys.executable, "-m", "unittest", "discover", "-s", "Tools/stage5_unified",
            "-p", "test_*.py"], "python_tests.txt")
    for label in ("a", "b"):
        folder = a.output / ("arm_" + label)
        logged([cmake, "--preset", "Stage5PDisplayReview", "-B", folder,
                "-DCMAKE_MAKE_PROGRAM=" + ninja], "arm_" + label + "_configure.txt", env)
        logged([cmake, "--build", folder, "-j", a.jobs], "arm_" + label + "_build.txt", env)
        binary = folder / "firmware_0x0530.bin"
        logged([arm / ("arm-none-eabi-objcopy" + suffix), "-O", "binary",
                folder / "stm32f103rbt6_a33.elf", binary], "objcopy_" + label + ".txt", env)
        report["arm"][label] = {"bytes": binary.stat().st_size, "sha256": sha(binary)}
    if report["arm"]["a"] != report["arm"]["b"]:
        raise ValueError("clean ARM builds differ")
    folder = a.output / "arm_a"
    analysis = a.output / "analysis"
    logged([sys.executable, ROOT / "Tools/stage5mr5c/analyze_ram_stack.py",
            "--elf", folder / "stm32f103rbt6_a33.elf", "--map", folder / "stm32f103rbt6_a33.map",
            "--callgraph-root", folder, "--output", analysis], "ram_analysis.txt", env)
    logged([sys.executable, ROOT / "Tools/stage5pa13d/nested_stack_gate.py",
            "--analysis", analysis, "--callgraph-root", folder,
            "--output", analysis / "nested_stack.json"], "stack_gate.txt", env)
    report["resources"] = json.loads((analysis / "nested_stack.json").read_text())
    for option in ("-print-libgcc-file-name", "-print-file-name=libg_nano.a", "-print-file-name=nano.specs"):
        path = Path(subprocess.check_output([str(compiler), "-mcpu=cortex-m3", "-mthumb", option], text=True).strip())
        report.setdefault("runtime_libraries", {})[option] = {"path": str(path), "sha256": sha(path)}
    (a.output / "qualification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"host": report["host"], "arm": report["arm"], "resources": report["resources"]}, indent=2))

if __name__ == "__main__":
    main()
