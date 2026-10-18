"""All capture tests use a fake client, never a COM port."""
import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

import capture_zero_return_0530 as capture
from test_zero_return_audit import batch, put


def authority():
    words = [0] * 40
    for index, value, size in [(0, 0xA13E0001, 2), (8, 1, 2),
            (10, 0x80000001, 2), (12, 0x80000002, 2), (14, -200000, 2),
            (16, 1, 2), (18, 100000, 4), (22, 300000, 4),
            (26, 300000, 4), (34, 2, 2), (36, 7, 2), (38, -200000, 2)]:
        put(words, index, value, size)
    return words


class CaptureTests(unittest.TestCase):
    def test_atomic_authority_signed_offset_and_unsigned_time(self):
        row = capture.decode_authority(authority())
        self.assertEqual(row["applied_offset_ug"], -200000)
        self.assertEqual(row["gross_ug"], 300000)
        self.assertEqual(row["sequence"], 0x80000001)
        self.assertEqual(row["uptime_ms"], 0x80000002)
        self.assertTrue(row["snapshot_atomic"])

    def test_apply_false_does_not_subtract_retained_offset(self):
        words = authority()
        put(words, 16, 0, 2)
        put(words, 22, 100000, 4)
        put(words, 26, 100000, 4)
        self.assertEqual(capture.decode_authority(words)["gross_ug"], 100000)

    def test_invalid_authority_equation_is_rejected(self):
        words = authority()
        put(words, 26, 1, 4)
        with self.assertRaisesRegex(ValueError, "equation"):
            capture.decode_authority(words)

    def test_fixed_reads_only_and_separate_sequence_labels(self):
        data = {b["address"]: b["words"] for b in batch()}
        data[0x20][27] = 1
        data[0x300] = [0xA13C, 2] + [0] * 35
        data[0x340] = authority()
        client = MagicMock()
        client.read.side_effect = lambda address, count: (data[address], None)
        row = capture.collect(client)
        self.assertEqual([x.args for x in client.read.call_args_list],
            [(0x340, 40), (0, 32), (0x20, 28), (0x1E0, 17), (0x280, 40), (0x300, 37)])
        self.assertFalse(client.write.called)
        self.assertFalse(row["observation_snapshot_atomic"])
        self.assertNotEqual(row["authority"]["gross_ug"], row["observation"]["gross_ug"])

    def test_identity_change_stops(self):
        data = {b["address"]: b["words"] for b in batch()}
        data[0][15] = 0x052E
        data[0x20][27] = 1
        data[0x300] = [0xA13C, 2] + [0] * 35
        data[0x340] = authority()
        client = MagicMock()
        client.read.side_effect = lambda address, count: (data[address], None)
        with self.assertRaisesRegex(ValueError, "identity"):
            capture.collect(client)

    def run_main(self, changed=False, order="high"):
        identity = dict(firmware="0x0530", map="0x010F", word_order=order,
            dirty=0, fault_mask=0, overrun_count=0, calibration_valid=1,
            revision=23, saved_revision=23,
            configuration=dict(active_profile=0, sample_rate=1, filter_mode=1,
                               filter_strength=3, gain=3),
            r5=dict(application=1, mode=2, limited=0, save_request_count_low=0),
            checkweigh=dict(mode=1))
        post = copy.deepcopy(identity)
        if changed:
            post["revision"] = post["saved_revision"] = 24
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "new"
            args = ["capture", "--port", "COM77", "--output", str(out), "--duration", "1"]
            with patch.object(sys, "argv", args), patch.object(capture, "SerialTransport") as transport, \
                    patch.object(capture, "ModbusClient"), \
                    patch.object(capture, "probe", side_effect=[identity, post]), \
                    patch.object(capture, "collect", return_value={"host_observed": True}), \
                    patch.object(capture.time, "monotonic", side_effect=[0, 0, 10]), \
                    patch.object(capture.time, "sleep"):
                rc = capture.main()
                self.assertEqual(transport.call_args.args[0], "COM77")
            return rc, json.loads((out / "summary.json").read_text()), (out / "samples.jsonl").read_text()

    def test_main_preserves_rows_and_declares_atomicity_scope(self):
        rc, summary, samples = self.run_main()
        self.assertEqual(rc, 0)
        self.assertEqual(summary["rows"], 1)
        self.assertTrue(summary["authority_snapshot_atomic"])
        self.assertFalse(summary["cross_request_snapshot_atomic"])
        self.assertTrue(samples)

    def test_main_configuration_change_retains_evidence_and_fails(self):
        rc, summary, samples = self.run_main(changed=True)
        self.assertEqual(rc, 2)
        self.assertEqual(summary["stopped_reason"], "STOPPED_ERROR")
        self.assertEqual(summary["rows"], 1)
        self.assertTrue(samples)

    def test_low_word_order_is_refused_before_collecting(self):
        rc, summary, samples = self.run_main(order="low")
        self.assertEqual(rc, 2)
        self.assertEqual(summary["rows"], 0)
        self.assertFalse(samples)


if __name__ == "__main__":
    unittest.main()
