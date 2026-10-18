"""Independent adversarial contracts for the offline evidence decoder."""
import csv
import json
import tempfile
import unittest
from pathlib import Path

import analyze_zero_return_0530 as audit


def put(words, start, value, size):
    raw = (value & ((1 << (size * 16)) - 1)).to_bytes(size * 2, "big")
    words[start:start + size] = [int.from_bytes(raw[i:i + 2], "big")
                                for i in range(0, len(raw), 2)]


def batch():
    blocks = [[0] * count for _, count in audit.REQUESTS]
    rt, dg, _, r5 = blocks
    rt[2:4] = [2, 1]
    rt[14:16] = [0x010F, 0x0530]
    put(rt, 16, -123000, 4)
    put(rt, 20, -123000, 4)
    put(dg, 0, 0x80000001, 2)
    put(dg, 2, 0x80000002, 2)
    r5[0:4] = [0x55B5, 1, 2, 5]
    put(r5, 6, -200000, 4)
    put(r5, 10, 100000, 4)
    put(r5, 14, 300000, 4)
    put(r5, 18, -380000, 4)
    return [dict(address=addr, count=count, words=words, tx_ns=0, rx_ns=10)
            for (addr, count), words in zip(audit.REQUESTS, blocks)]


def packet(payload):
    return payload + audit.crc16(payload).to_bytes(2, "little")


def frame_text(blocks):
    text = []
    for block in blocks:
        tx = packet(b"\x01\x03" + block["address"].to_bytes(2, "big") +
                    block["count"].to_bytes(2, "big"))
        rx = packet(bytes((1, 3, block["count"] * 2)) + b"".join(
            value.to_bytes(2, "big") for value in block["words"]))
        text.extend(("T 0 TX " + tx.hex(" "), "T 10 RX " + rx.hex(" ")))
    return "\n".join(text) + "\n"


def synthetic_rows():
    rows = []
    for loaded, seconds in [(True, 60), (False, 120), (True, 120),
                            (False, 120), (True, 120), (False, 120), (True, 60)]:
        for _ in range(seconds * 10):
            row = {field: 0 for field in audit.STAT_FIELDS}
            row.update(utc=str(len(rows)), host_monotonic_ns=len(rows) * 100_000_000,
                       net_ug=500_380_000 if loaded else 380_000,
                       stable=1, r5_state=5, display_state=2)
            rows.append(row)
    return rows


class AuditTests(unittest.TestCase):
    def test_known_modbus_request_crc(self):
        self.assertEqual(audit.crc16(bytes.fromhex("01 03 02 80 00 28")), 0x8445)

    def test_signed_offsets_and_unsigned_ticks_and_nonatomic_requests(self):
        row = audit.decode_batch(batch())
        self.assertEqual(row["r5_offset_ug"], -200000)
        self.assertEqual(row["r5_reference_ug"], -380000)
        self.assertEqual(row["sequence"], 0x80000001)
        self.assertEqual(row["uptime_ms"], 0x80000002)
        self.assertEqual(row["r5_equation_error_ug"], 0)
        self.assertNotEqual(row["r5_corrected_ug"], row["gross_ug"])

    def test_request_order_is_required(self):
        with self.assertRaises(ValueError):
            audit.decode_batch(list(reversed(batch())))

    def test_raw_and_csv_values_are_joined_and_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "frames.txt").write_text(frame_text(batch()))
            row = audit.decode_batch(batch())
            fields = audit.CSV_FIELDS
            def write(value):
                with (root / "samples.csv").open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, fields)
                    writer.writeheader()
                    item = {key: row[key] for key in fields if key not in ("utc", "host_monotonic_ns")}
                    item.update(utc="T", host_monotonic_ns=10, net_ug=value)
                    writer.writerow(item)
                (root / "summary.json").write_text(json.dumps(dict(rows=1, read_errors=0,
                    samples_sha256=audit.digest(root / "samples.csv"))))
            write(row["net_ug"])
            self.assertEqual(len(audit.load_run(root, 0x0530)), 1)
            write(row["net_ug"] + 1)
            with self.assertRaisesRegex(ValueError, "raw/CSV mismatch"):
                audit.load_run(root, 0x0530)

    def test_corrupted_crc_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "frames"
            path.write_text(frame_text(batch()).replace("01 03", "01 04", 1))
            with self.assertRaisesRegex(ValueError, "CRC"):
                list(audit.exchanges(path))

    def test_crc_valid_write_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "frames"
            path.write_text("T 0 TX " + packet(bytes.fromhex("01 06 00 00 00 01")).hex(" "))
            with self.assertRaisesRegex(ValueError, "non-FC03"):
                list(audit.exchanges(path))

    def test_unpaired_request_or_response_is_rejected(self):
        lines = frame_text(batch()).splitlines()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "frames"
            for text in (lines[0], lines[1]):
                path.write_text(text)
                with self.assertRaises(ValueError):
                    list(audit.exchanges(path))

    def test_only_two_complete_cycles_and_partial_tail_not_promoted(self):
        edges = audit.edge_analysis(synthetic_rows())
        self.assertEqual(len(edges["events"]), 6)
        self.assertEqual(len(edges["complete_empty_load_empty_cycles"]), 2)
        self.assertFalse(edges["events"][-1]["post_90_120"]["complete"])

    def test_mixed_plateau_is_not_a_complete_comparison(self):
        rows = synthetic_rows()
        window = audit.window(rows, 60_000_000_000, -1, 1, False)
        self.assertFalse(window["complete"])
        self.assertEqual(window["status"], "MIXED_LOAD_STATES")

    def test_long_poll_gap_is_not_hidden_by_sample_count(self):
        rows = synthetic_rows()
        rows = [row for row in rows if not 80e9 < row["host_monotonic_ns"] < 81e9]
        self.assertFalse(audit.window(rows, 60_000_000_000, 15, 45, False)["complete"])

    def test_means_decompose_without_claiming_medians_are_additive(self):
        before, after = [], []
        for values, target in [([(0, 0), (0, 0), (0, 0)], before),
                               ([(100, 0), (100, 100), (1000, 100)], after)]:
            for i, (raw, offset) in enumerate(values):
                row = {field: 0 for field in audit.STAT_FIELDS}
                row.update(utc=str(i), stable=1, r5_state=5, display_state=2,
                    r5_uncompensated_ug=raw, r5_offset_ug=offset, r5_corrected_ug=raw-offset)
                target.append(row)
        a = dict(audit.describe(before), complete=True)
        b = dict(audit.describe(after), complete=True)
        result = audit.compare(a, b)
        self.assertAlmostEqual(result["r5_mean_decomposition_error_ug"], 0)
        d = result["delta"]
        self.assertNotEqual(d["r5_corrected_ug"]["median"],
                            d["r5_uncompensated_ug"]["median"] - d["r5_offset_ug"]["median"])


if __name__ == "__main__":
    unittest.main()
