"""Interactive HF2-R1 hardware regression with evidence-backed results."""

import csv
import json
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

import register_map as reg
from hf2_status import read_status
from hw_common import HardwareTestError, execute_command, probe_device


RESULT_STATES = {"PASS", "FAIL", "SKIPPED", "NOT TESTED",
                 "MANUAL PASS", "MANUAL FAIL"}


class HF2RegressionReport:
    def __init__(self, root, interface="rs485", enabled=True):
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        root = Path(root) if root else Path("Results/hardware")
        self.directory = root / ("hf2_r1_%s" % stamp)
        self.enabled = enabled
        self.results = []
        self.latencies = []
        self._finished = False
        if enabled:
            self.directory.mkdir(parents=True, exist_ok=False)
            self.raw_path = self.directory / "raw_frames.log"
            self.raw_path.touch()

    def raw(self, line):
        if self.enabled:
            with self.raw_path.open("a", encoding="utf-8") as stream:
                stream.write(datetime.now(timezone.utc).isoformat() + " " + line + "\n")

    def add(self, name, status, detail="", data=None):
        legacy = {
            "NOT VERIFIED WITH CURRENT EQUIPMENT": "NOT TESTED",
            "REQUIRES LOGIC ANALYZER": "NOT TESTED",
            "REQUIRES ADJUSTABLE POWER SUPPLY": "NOT TESTED",
        }
        if status in legacy:
            detail = (status + (": " + detail if detail else ""))
            status = legacy[status]
        if status not in RESULT_STATES:
            raise ValueError("invalid HF2-R1 result state: %s" % status)
        item = {"name": name, "status": status, "detail": detail,
                "timestamp_utc": datetime.now(timezone.utc).isoformat()}
        if data is not None:
            item["data"] = data
        self.results.append(item)
        return status in ("PASS", "MANUAL PASS")

    def latency(self, row):
        self.latencies.append(dict(row))

    def write_json(self, name, value):
        if self.enabled:
            (self.directory / name).write_text(json.dumps(
                value, indent=2, ensure_ascii=False, default=str), encoding="utf-8")

    def write_csv(self, name, rows):
        rows = list(rows)
        if not self.enabled or not rows:
            return
        keys = sorted({key for row in rows for key in row})
        with (self.directory / name).open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, keys)
            writer.writeheader()
            writer.writerows(rows)

    def finish(self):
        if not self.enabled or self._finished:
            return self.directory if self.enabled else None
        self._finished = True
        self.write_json("HF2_R1_HARDWARE_REGRESSION.json", self.results)
        lines = ["# HF2-R1 Hardware Regression", ""]
        for item in self.results:
            lines.append("- **%s** %s: %s" %
                         (item["status"], item["name"], item["detail"]))
        lines.extend(["", "Runtime drift compensation remains DEFAULT DISABLED.",
                      "NOT METROLOGICALLY VALIDATED."])
        (self.directory / "HF2_R1_HARDWARE_REGRESSION.md").write_text(
            "\n".join(lines) + "\n", encoding="utf-8")
        return self.directory


def _flat(value, prefix=""):
    result = {}
    for key, item in value.items():
        name = (prefix + "." + key) if prefix else key
        if isinstance(item, dict):
            result.update(_flat(item, name))
        else:
            result[name] = item
    return result


def _capture(client, report, name, duration_s, interval_s=0.1):
    rows = []
    deadline = time.monotonic() + duration_s
    try:
        while time.monotonic() < deadline:
            row = _flat(read_status(client))
            row["host_timestamp_utc"] = datetime.now(timezone.utc).isoformat()
            rows.append(row)
            time.sleep(interval_s)
    finally:
        report.write_csv(name, rows)
    return rows


def _capture_action(client, report, name, prompt, timeout_s=20,
                    post_confirm_s=5, interval_s=0.1, input_fn=input):
    confirmed = threading.Event()

    def wait_for_operator():
        input_fn(prompt + "\nPress Enter immediately after placing the load: ")
        confirmed.set()

    thread = threading.Thread(target=wait_for_operator, daemon=True)
    thread.start()
    rows = []
    start = time.monotonic()
    confirmed_at = None
    try:
        while time.monotonic() - start < timeout_s:
            now = time.monotonic()
            if confirmed.is_set() and confirmed_at is None:
                confirmed_at = now
            row = _flat(read_status(client))
            row["host_elapsed_ms"] = int((now - start) * 1000)
            row["operator_confirmed"] = confirmed.is_set()
            rows.append(row)
            if confirmed_at is not None and now - confirmed_at >= post_confirm_s:
                break
            time.sleep(interval_s)
    finally:
        report.write_csv(name, rows)
    if not confirmed.is_set():
        raise HardwareTestError("operator action timed out")
    return rows


def _last(rows, key, default=None):
    return rows[-1].get(key, default) if rows else default


def _operator_ready(text, input_fn=input):
    input_fn(text + "\nPress Enter only after the physical action is complete: ")


def _manual_observation(report, name, prompt, input_fn=input):
    answer = input_fn(prompt + " [PASS/FAIL/OBSERVATION]: ").strip()
    upper = answer.upper()
    if upper == "PASS":
        return report.add(name, "MANUAL PASS", "operator observation")
    if upper == "FAIL":
        return report.add(name, "MANUAL FAIL", "operator observation")
    report.add(name, "NOT TESTED", answer or "no observation supplied")
    return False


def should_offer_save(args):
    return not args.skip_flash


def should_run_drift(args):
    return not args.skip_drift


def verify_runtime_boundaries(client, report):
    reserved = client.read(reg.RUNTIME_DRIFT_RESERVED, 1)[0][0]
    block = client.read(reg.RUNTIME_DRIFT_FIRST,
                        reg.RUNTIME_DRIFT_LAST - reg.RUNTIME_DRIFT_FIRST + 1)[0]
    last = client.read(reg.RUNTIME_DRIFT_LAST, 1)[0][0]
    ok = reserved == 0 and block[3] == 0 and len(block) == 30
    report.write_json("register_0200_021d.json", {
        "first": reg.RUNTIME_DRIFT_FIRST, "last": reg.RUNTIME_DRIFT_LAST,
        "values": block, "reserved": reserved, "last_value": last})
    report.add("0203 RESERVED", "PASS" if ok else "FAIL",
               "read-as-zero and whole block aligned", {"value": reserved})
    try:
        client.read(reg.RUNTIME_DRIFT_LAST + 1, 1)
    except Exception as exc:
        illegal = "0x02" in str(exc)
        report.add("021E boundary", "PASS" if illegal else "FAIL", str(exc))
        ok &= illegal
    else:
        report.add("021E boundary", "FAIL", "illegal address was accepted")
        ok = False
    return ok


def run_readonly(client, report, smoke_count=20):
    from test_smoke import run as run_smoke
    identity = probe_device(client)
    report.write_json("probe.json", identity)
    report.add("PROBE", "PASS", "HF2-R1 identity", identity)
    status = read_status(client)
    report.write_json("hf2_status.json", status)
    report.add("HF2-R1 telemetry", "PASS", "read-only status decoded")
    if not verify_runtime_boundaries(client, report):
        return False
    return run_smoke(client, report, smoke_count, 0.05)


def _command(client, report, name, arg0=0):
    token = int(time.time() * 1000) & 0xFFFF or 1
    response = execute_command(client, token, reg.COMMANDS[name], arg0=arg0)
    passed = response["result"] in (0, 1)
    report.add("command " + name, "PASS" if passed else "FAIL",
               response["result_name"], {"arg0": arg0, "response": response})
    if not passed:
        raise HardwareTestError("command %s rejected: %s" %
                                (name, response["result_name"]))
    return response


def run_interactive(client, report, args, input_fn=input):
    import soak_test
    if not run_readonly(client, report, args.smoke_count):
        return False
    _command(client, report, "RUNTIME_DRIFT_CONTROL", 0)
    disabled = read_status(client)["runtime_drift"]
    disabled_ok = not disabled["enabled"] and disabled["state"]["name"] == "DISABLED"
    report.add("drift default/disable", "PASS" if disabled_ok else "FAIL",
               "runtime drift disabled before ordinary weighing", disabled)
    if not disabled_ok:
        return False

    _operator_ready("Remove all test objects and wait for a stable display.", input_fn)
    empty = _capture(client, report, "weighing_empty.csv", 10)
    empty_ok = bool(empty) and any(row.get("weighing.stable") for row in empty)
    report.add("empty load", "PASS" if empty_ok else "FAIL",
               "stable sample observed" if empty_ok else "no stable sample")
    if not empty_ok:
        return False

    _operator_ready("Place the approximately 500 g test weight and keep it still.", input_fn)
    loaded = _capture(client, report, "weighing_500g.csv", 30)
    loaded_ok = bool(loaded) and any(row.get("weighing.stable") for row in loaded)
    report.add("500 g", "PASS" if loaded_ok else "FAIL", "stable load observed")
    if not loaded_ok:
        return False

    baseline = _last(loaded, "weighing.gross.ug", 0)
    plus = _capture_action(client, report, "weighing_plus_1g.csv",
                           "Add approximately 1 g without pressing the load cell.",
                           input_fn=input_fn)
    delta = max((row.get("weighing.gross.ug", baseline) - baseline for row in plus),
                default=0)
    unlocked = any(not row.get("display_conditioner.locked", True) for row in plus)
    plus_ok = delta > 500000 and unlocked
    report.add("500 g + 1 g", "PASS" if plus_ok else "FAIL",
               "authoritative delta and display release", {"delta_ug": delta,
                                                            "unlocked": unlocked})
    if not plus_ok:
        return False

    _operator_ready("Clear the platform and wait stable.", input_fn)
    tare_rows = _capture_action(client, report, "tare_quick_load.csv",
        "Short-press TARE, then quickly add about 1 g without pressing the load cell.",
        input_fn=input_fn)
    tare_ok = any(row.get("weighing.tare_active") for row in tare_rows) and \
        (max((row.get("weighing.net.ug", 0) for row in tare_rows), default=0) -
         min((row.get("weighing.net.ug", 0) for row in tare_rows), default=0) > 500000)
    report.add("TARE quick +1 g", "PASS" if tare_ok else "FAIL",
               "tare active and net change observed")
    _manual_observation(report, "TARE panel", "Did the panel leave 0.00 and show the added load?", input_fn)

    _operator_ready("Long-press TARE to clear tare, clear the platform, and wait stable.", input_fn)
    zero_rows = _capture_action(client, report, "zero_quick_load.csv",
        "Short-press ZERO, then quickly add about 1 g without pressing the load cell.",
        input_fn=input_fn)
    zero_ok = bool(zero_rows) and not _last(zero_rows, "weighing.tare_active", True) and \
        (max((row.get("weighing.gross.ug", 0) for row in zero_rows), default=0) -
         min((row.get("weighing.gross.ug", 0) for row in zero_rows), default=0) > 500000)
    report.add("ZERO quick +1 g", "PASS" if zero_ok else "FAIL",
               "tare clear and gross change observed")
    _manual_observation(report, "ZERO panel", "Did the panel release 0.00 and show the added load?", input_fn)

    _operator_ready("Clear the platform, short-press TARE, then short-press ZERO.", input_fn)
    tare_zero = read_status(client)
    report.add("TARE active ZERO automatic", "PASS" if tare_zero["weighing"]["tare_active"] else "FAIL",
               "tare remains active; zero-offset is not Modbus mapped")
    _manual_observation(report, "TARE active ZERO panel",
                        "Did the panel show the tare-active rejection message?", input_fn)
    _operator_ready("Long-press TARE to clear tare.", input_fn)

    report.add("OL migration", "SKIPPED",
               "old 10 kg configuration presence cannot be reconstructed and OL is not Modbus mapped")
    _manual_observation(report, "OL menu",
                        "Can OL be edited without remaining on UnItHI?", input_fn)
    if not should_offer_save(args):
        report.add("SAVE power cycle", "SKIPPED", "--skip-flash supplied")
    else:
        confirmation = input_fn("Type CONFIRM FLASH SAVE to request persistent SAVE: ").strip()
        if confirmation != "CONFIRM FLASH SAVE":
            report.add("SAVE power cycle", "SKIPPED", "explicit confirmation not supplied")
        else:
            _command(client, report, "REQUEST_SAVE")
            _operator_ready("Power off normally for at least 3 seconds, then power on.", input_fn)
            saved = probe_device(client)
            report.add("SAVE power cycle", "PASS", "post-cycle probe succeeded", saved)

    if not should_run_drift(args):
        report.add("runtime drift hardware", "SKIPPED", "--skip-drift supplied")
    else:
        _command(client, report, "RUNTIME_DRIFT_CONTROL", 1)
        try:
            enabled = read_status(client)["runtime_drift"]
            report.add("drift enable", "PASS" if enabled["enabled"] else "FAIL",
                       "volatile command 25", enabled)
            _operator_ready("Place a fixed approximately 500 g load and do not touch it for at least 310 seconds.", input_fn)
            arming = _capture(client, report, "runtime_drift_arming.csv", 310, 0.5)
            tracking = _last(arming, "runtime_drift.state.name") == "TRACKING"
            report.add("drift ARMING/TRACKING", "PASS" if tracking else "FAIL",
                       "five-minute stable arming", {"samples": len(arming)})
            if tracking:
                windows = _capture(client, report, "runtime_drift_tracking.csv", 90, 0.5)
                report.add("drift tracking window", "PASS", "90-second observation",
                           {"final_offset_ug": _last(windows, "runtime_drift.offset.ug")})
                before = _last(windows, "runtime_drift.offset.ug", 0)
                base = _last(windows, "runtime_drift.uncompensated_gross.ug", 0)
                protection = _capture_action(client, report,
                    "runtime_drift_load_protection.csv",
                    "Add approximately 1 g without pressing the load cell.",
                    input_fn=input_fn)
                after = _last(protection, "runtime_drift.offset.ug", before)
                gross_delta = max((row.get("runtime_drift.uncompensated_gross.ug", base) - base
                                   for row in protection), default=0)
                frozen = any(row.get("runtime_drift.state.name") == "FROZEN" for row in protection)
                protected = gross_delta > 500000 and after == before and frozen
                report.add("1 g drift protection", "PASS" if protected else "FAIL",
                           "real load must freeze without offset absorption",
                           {"delta_ug": gross_delta, "offset_before": before,
                            "offset_after": after, "frozen": frozen})
                _operator_ready("Restore a fixed stable load, wait stable, then short-press TARE and leave tare active.", input_fn)
                rearm = _capture(client, report, "runtime_drift_tare_rearm.csv", 330, 0.5)
                rearmed = _last(rearm, "weighing.tare_active", False) and \
                    _last(rearm, "runtime_drift.state.name") == "TRACKING"
                report.add("TARE active rearm", "PASS" if rearmed else "FAIL",
                           "tare remains active and drift returns to TRACKING")
                _operator_ready("Long-press TARE to clear tare.", input_fn)
        finally:
            _command(client, report, "RUNTIME_DRIFT_CONTROL", 0)

    from test_smoke import run as run_smoke
    smoke_ok = run_smoke(client, report, 100, 0.05)
    soak_path = report.directory / "rs485_soak.csv"
    soak_ok = soak_test.run(client, report, 600, 50, 3, 0,
                            str(soak_path), timeout_retries=1)
    return smoke_ok and soak_ok and all(
        item["status"] not in ("FAIL", "MANUAL FAIL") for item in report.results)


def run(client, report, args, input_fn=input):
    if args.non_interactive_readonly:
        return run_readonly(client, report, args.smoke_count)
    return run_interactive(client, report, args, input_fn)
