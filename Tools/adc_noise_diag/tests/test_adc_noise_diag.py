import csv
import tempfile
from pathlib import Path
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
STAGE5B = ROOT.parent / "stage5b_hw"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(STAGE5B))

import modbus_frame
import analyze_adc_noise
import capture_adc_noise
import compare_noise_runs
from diag_common import (U32SequenceTracker, U32TimeExtender,
                         decode_cs1237_config, load_csv_rows,
                         validate_diagnostic_identity)


def sample_row(index, raw, excluded="false"):
    return {
        "elapsed_s": str(index * 0.1),
        "device_timestamp_ms": str(index * 100),
        "raw_count": str(raw), "filtered_raw_count": str(raw),
        "sample_sequence": str(index), "lost_samples": "0",
        "overrun_count": "0", "excluded": excluded,
        "test_id": "synthetic", "mode": "channel_a", "load_g": "0",
        "counts_per_g": "1000", "notes": "test",
    }


class FakeTransport:
    def __init__(self, *_args, **_kwargs):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def reset(self):
        pass


class DiagnosticToolTests(unittest.TestCase):
    def test_config_identity(self):
        decoded = decode_cs1237_config(0x0F)
        self.assertEqual(decoded["channel"], "internal_short")
        self.assertEqual(decoded["rate_hz"], 10)
        self.assertEqual(decoded["gain"], 128)
        self.assertTrue(decoded["reference_output_enabled"])
        validate_diagnostic_identity("internal_short", 1, 0x0F)
        validate_diagnostic_identity("channel_a", 2, 0x0C)
        with self.assertRaises(ValueError):
            validate_diagnostic_identity("internal_short", 2, 0x0C)
        with self.assertRaises(ValueError):
            validate_diagnostic_identity("channel_a", 2, 0x4C)

    def test_signed_word_order(self):
        self.assertEqual(modbus_frame.decode_i32_words([0xFFFF, 0xFFF6], "high"), -10)
        self.assertEqual(modbus_frame.decode_i32_words([0xFFF6, 0xFFFF], "low"), -10)

    def test_sequence_dedup_loss_and_wrap(self):
        tracker = U32SequenceTracker()
        self.assertTrue(tracker.update(0xFFFFFFFE).accepted)
        self.assertFalse(tracker.update(0xFFFFFFFE).accepted)
        update = tracker.update(1)
        self.assertEqual(update.delta, 3)
        self.assertEqual(update.lost, 2)
        self.assertEqual(tracker.total_lost, 2)

    def test_timestamp_wrap(self):
        extender = U32TimeExtender()
        first = extender.update(0xFFFFFFF0)
        second = extender.update(0x00000020)
        self.assertEqual(second - first, 0x30)

    def test_warmup_exclusion_and_statistics(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.csv"
            rows = [sample_row(0, 10, "true"), sample_row(1, 11),
                    sample_row(2, 12), sample_row(3, 13)]
            with path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                writer.writeheader(); writer.writerows(rows)
            loaded = load_csv_rows(path)
            self.assertEqual(len(loaded), 3)
            stats, _, _, _ = analyze_adc_noise.compute_statistics(loaded)
            self.assertEqual(stats["valid_samples"], 3)
            self.assertEqual(stats["peak_to_peak_count"], 2)
            self.assertAlmostEqual(stats["equivalent_peak_to_peak_g"], 0.002)

    def test_variance_decomposition(self):
        runs = [
            {"test_id": "A_internal_short", "mode": "internal_short",
             "detrended_std_count": 2.0},
            {"test_id": "B_external_common_mode", "mode": "channel_a",
             "detrended_std_count": 3.0},
            {"test_id": "C_dummy_bridge", "mode": "channel_a",
             "detrended_std_count": 5.0},
            {"test_id": "D_sensor_zero", "mode": "channel_a",
             "detrended_std_count": 6.0},
        ]
        layers = compare_noise_runs.layered_variance(runs)
        self.assertEqual(layers["var_adc"], 4.0)
        self.assertEqual(layers["var_pcb_increment"], 5.0)
        self.assertEqual(layers["var_bridge_supply_increment"], 16.0)
        self.assertEqual(layers["real_sensor"][0]["var_sensor_mechanical_increment"], 11.0)

    def test_analysis_and_comparison_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            json_inputs = []
            for run, mode in (("A_internal_short", "internal_short"),
                              ("B_external_common_mode", "channel_a")):
                csv_path = root / (run + ".csv")
                rows = [sample_row(index, 100 + (index % 7) +
                                   (0 if mode == "internal_short" else index % 3))
                        for index in range(64)]
                for row in rows:
                    row["test_id"] = run; row["mode"] = mode
                with csv_path.open("w", newline="", encoding="utf-8") as stream:
                    writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                    writer.writeheader(); writer.writerows(rows)
                analysis_dir = root / (run + "_analysis")
                self.assertEqual(analyze_adc_noise.main(
                    [str(csv_path), "--output-dir", str(analysis_dir)]), 0)
                json_inputs.append(analysis_dir / (run + ".json"))
                self.assertTrue((analysis_dir / "psd_0_5hz.png").exists())
                self.assertTrue((analysis_dir / "allan_deviation.png").exists())
            comparison = root / "comparison"
            self.assertEqual(compare_noise_runs.main(
                [str(path) for path in json_inputs] +
                ["--output-dir", str(comparison)]), 0)
            self.assertTrue((comparison / "ADC_NOISE_COMPARISON.md").exists())
            self.assertTrue((comparison / "comparison_std.png").exists())

    def test_ctrl_c_preserves_atomic_csv(self):
        block = [0] * 33
        block[0:2] = [0xFFFF, 0xFFF6]
        block[2:4] = [0xFFFF, 0xFFF6]
        block[4:6] = [0, 1]
        block[6:8] = [0, 100]
        block[15] = 4
        block[16] = 0
        block[17:19] = [0, 0]
        block[19] = 0x0F
        block[32] = 1
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture.csv"
            args = ["--port", "COM1", "--mode", "internal_short",
                    "--duration", "1", "--warmup", "0", "--test-id", "ctrlc",
                    "--output", str(output)]
            with mock.patch.object(capture_adc_noise, "SerialTransport", FakeTransport), \
                    mock.patch.object(capture_adc_noise, "read_identity",
                                      return_value=("high", {}, block)), \
                    mock.patch.object(capture_adc_noise, "calibration_counts_per_g",
                                      return_value=None), \
                    mock.patch.object(capture_adc_noise, "read_with_retry",
                                      side_effect=[block, KeyboardInterrupt()]):
                self.assertEqual(capture_adc_noise.main(args), 130)
            self.assertTrue(output.exists())
            self.assertFalse(output.with_suffix(".csv.tmp").exists())
            with output.open("r", newline="", encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["raw_count"], "-10")


if __name__ == "__main__":
    unittest.main()
