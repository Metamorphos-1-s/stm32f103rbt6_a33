import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import register_map as reg
from hf2_r1_regression import (HF2RegressionReport, _capture,
    _manual_observation, run_readonly, should_offer_save, should_run_drift)
from hw_common import HardwareTestError
from serial_transport import Exchange


class ReadOnlyClient:
    def __init__(self):
        self.slave = 1
        self.writes = 0

    def write_single(self, *_):
        self.writes += 1
        raise AssertionError("read-only regression attempted FC06")

    def write_multiple(self, *_):
        self.writes += 1
        raise AssertionError("read-only regression attempted FC16")

    def read(self, address, quantity):
        if address == reg.RUNTIME_DRIFT_LAST + 1:
            raise HardwareTestError("Modbus exception 0x02")
        values = [0] * quantity
        if address == reg.REALTIME_FIRST and quantity == 0x20:
            values[14] = reg.REGISTER_MAP_VERSION
            values[15] = reg.FIRMWARE_VERSION
        elif address == reg.STORAGE_FIRST:
            values[0] = reg.SCHEMA_VERSION
        elif address == reg.ACTIVE_WORD_ORDER:
            values[0] = 0
        elif address == reg.RUNTIME_DRIFT_RESERVED:
            values[0] = 0
        exchange = Exchange(b"", b"", 1.0, 2.0)
        return values, exchange


class RecordingReport:
    def __init__(self):
        self.results = []
        self.csv_rows = None

    def add(self, name, status, detail="", data=None):
        self.results.append({"name": name, "status": status, "detail": detail})
        return status == "PASS"

    def write_json(self, *_):
        pass

    def write_csv(self, _name, rows):
        self.csv_rows = list(rows)

    def latency(self, *_):
        pass


class RegressionTests(unittest.TestCase):
    def test_readonly_mode_never_writes(self):
        client = ReadOnlyClient()
        report = RecordingReport()
        self.assertTrue(run_readonly(client, report, smoke_count=2))
        self.assertEqual(client.writes, 0)
        self.assertIn("PASS", [item["status"] for item in report.results])

    def test_skip_options_suppress_save_and_drift(self):
        args = SimpleNamespace(skip_flash=True, skip_drift=True)
        self.assertFalse(should_offer_save(args))
        self.assertFalse(should_run_drift(args))

    def test_manual_observation_is_not_automatic_pass(self):
        report = RecordingReport()
        self.assertFalse(_manual_observation(report, "panel", "prompt",
                                             lambda _: "looks odd"))
        self.assertEqual(report.results[-1]["status"], "NOT TESTED")

    def test_ctrl_c_preserves_captured_csv(self):
        report = RecordingReport()
        with mock.patch("hf2_r1_regression.read_status",
                        side_effect=[{"runtime": {"state": 1}}, KeyboardInterrupt]):
            with self.assertRaises(KeyboardInterrupt):
                _capture(object(), report, "partial.csv", 10, 0)
        self.assertEqual(len(report.csv_rows), 1)

    def test_report_rejects_ambiguous_status(self):
        with tempfile.TemporaryDirectory() as directory:
            report = HF2RegressionReport(Path(directory), enabled=True)
            with self.assertRaises(ValueError):
                report.add("step", "COMPLETE")


if __name__ == "__main__":
    unittest.main()
