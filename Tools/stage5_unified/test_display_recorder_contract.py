"""Recorder contract tests use a fake transport; no device is opened."""
import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch
import record_display_diag_052c as recorder

def words(value, count, order):
    value &= (1 << (16 * count)) - 1
    result = [(value >> (16 * i)) & 65535 for i in reversed(range(count))]
    return result if order == "high" else result[::-1]

def blocks(order, firmware):
    rt, diag, disp, r5 = [0] * 32, [0] * 28, [0] * 17, [0] * 40
    rt[0:2] = words(-123, 2, order)
    rt[4] = 0x10
    rt[14:16] = [0x010F, firmware]
    rt[16:20] = words(-2300000, 4, order)
    rt[20:24] = words(500000000, 4, order)
    rt[28:30] = words(-987, 2, order)
    diag[0:2] = words(0x80000001, 2, order)
    diag[2:4] = words(0x80000002, 2, order)
    disp[2:6] = words(-1230000, 4, order)
    r5[6:10] = words(-1587, 4, order)
    return {0: rt, 0x20: diag, 0x1e0: disp, 0x280: r5}

class RecorderContract(unittest.TestCase):
    def capture(self, output, order="high", firmware=0x0530):
        fake = MagicMock()
        data = blocks(order, firmware)
        fake.read.side_effect = lambda start, count: (data[start], None)
        args = ["recorder", "--output", str(output), "--duration", "1",
                "--port", "COM77", "--firmware", "0x0530", "--word-order", order]
        with patch.object(sys, "argv", args), patch.object(recorder, "SerialTransport") as transport, \
                patch.object(recorder, "ModbusClient", return_value=fake), \
                patch.object(recorder.time, "monotonic", side_effect=[0, 0, 10]), \
                patch.object(recorder.time, "sleep"):
            recorder.main()
            self.assertEqual(transport.call_args.args[0], "COM77")
        self.assertEqual([call.args for call in fake.read.call_args_list],
                         [(0, 32), (0x20, 28), (0x1e0, 17), (0x280, 40)])
        self.assertFalse(fake.write.called)
        return json.loads((output / "summary.json").read_text())

    def test_port_word_order_signed_weights_and_unsigned_sequence(self):
        for order in ("high", "low"):
            with tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp) / "capture"
                summary = self.capture(out, order)
                self.assertFalse(summary["snapshot_atomic"])
                self.assertEqual(summary["rows"], 1)
                with (out / "samples.csv").open(newline="") as f:
                    row = next(csv.DictReader(f))
                self.assertEqual(int(row["display_count"]), -123)
                self.assertEqual(int(row["net_ug"]), -2300000)
                self.assertEqual(int(row["raw_adc"]), -987)
                self.assertEqual(int(row["r5_offset_ug"]), -1587)
                self.assertEqual(int(row["sequence"]), 0x80000001)
                self.assertEqual(int(row["uptime_ms"]), 0x80000002)

    def test_identity_change_stops_instead_of_repeated_polling(self):
        with tempfile.TemporaryDirectory() as tmp:
            summary = self.capture(Path(tmp) / "capture", firmware=0x052E)
            self.assertEqual(summary["rows"], 0)
            self.assertEqual(summary["read_errors"], 1)
            self.assertEqual(summary["stopped_reason"], "IDENTITY_CHANGED")

if __name__ == "__main__":
    unittest.main()
