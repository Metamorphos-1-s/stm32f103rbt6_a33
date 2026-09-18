import importlib.util
import tempfile
import unittest
from pathlib import Path


SPEC = importlib.util.spec_from_file_location("analyze_r5d", Path(__file__).with_name("analyze_r5d.py"))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class AnalyzeR5DTests(unittest.TestCase):
    def test_median_even_and_odd(self):
        self.assertEqual(MODULE.median([3, 1, 2]), 2)
        self.assertEqual(MODULE.median([4, 1, 3, 2]), 2.5)

    def test_ols_slope_per_hour(self):
        self.assertAlmostEqual(MODULE.ols_slope_per_hour([(0, 10), (3600, 12), (7200, 14)]), 2.0)

    def test_parse_utc(self):
        self.assertEqual(MODULE.parse_utc("1970-01-01T00:00:01.000Z"), 1.0)

    def test_read_rows_preserves_hex_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "samples.csv"
            path.write_text("utc,firmware,value\n1970-01-01T00:00:01.000Z,0x0511,7\n", encoding="utf-8")
            row = next(MODULE.read_rows(path))
            self.assertEqual(row["firmware"], "0x0511")
            self.assertEqual(row["value"], 7)

    def test_write_json_uses_final_lf(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "value.json"
            MODULE.write_json(path, {"ok": True})
            self.assertTrue(path.read_bytes().endswith(b"\n"))


if __name__ == "__main__":
    unittest.main()
