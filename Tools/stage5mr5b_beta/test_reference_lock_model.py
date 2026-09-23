#!/usr/bin/env python3
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from reference_lock_model import Mode, Reason, ReferenceLock, State, median_int, trunc_div


class ReferenceLockUnitTests(unittest.TestCase):
    def test_signed_truncation_and_median_contract(self):
        self.assertEqual(trunc_div(-5, 2), -2)
        self.assertEqual(median_int([-4, -3]), -3)
        self.assertEqual(median_int([3, 4]), 3)

    def test_off_does_not_apply_offset(self):
        model = ReferenceLock(); model.offset_ug = 200_000; model.offset_milli_ug = 200_000_000
        snap = model.process_second(0, 500_000_000)
        self.assertEqual(snap["corrected_gross_ug"], 500_000_000)

    def test_dosing_preserves_and_freezes_offset(self):
        model = ReferenceLock(); model.offset_ug = 200_000; model.offset_milli_ug = 200_000_000
        model.set_mode(Mode.DOSING_NO_COMPENSATION)
        for second in range(1000):
            snap = model.process_second(second, second * 20_000)
        self.assertEqual(snap["offset_ug"], 200_000)
        self.assertEqual(snap["corrected_gross_ug"], 19_780_000)

    def test_zero_clears_offset_but_tare_does_not_need_core_event(self):
        model = ReferenceLock(); model.offset_ug = 123_456; model.offset_milli_ug = 123_456_000
        model.set_mode(Mode.DOSING_NO_COMPENSATION)
        model.reset(Reason.ZERO)
        self.assertEqual(model.offset_ug, 0)
        self.assertEqual(model.state, State.DOSING)

    def test_fault_limits_without_changing_offset(self):
        model = ReferenceLock(); model.offset_ug = 1000; model.offset_milli_ug = 1_000_000
        model.set_mode(Mode.STATIC_COMPENSATION)
        snap = model.process_second(0, 0, fault=True)
        self.assertEqual(snap["offset_ug"], 1000)
        self.assertEqual(snap["state"], State.LIMITED)

    def test_profile_change_rebaselines_once_and_preserves_offset(self):
        model = ReferenceLock()
        model.set_mode(Mode.STATIC_COMPENSATION)
        model.offset_ug = -175_000
        model.offset_milli_ug = -175_000_000
        model.process_sample(100, 975, 500_000_000)
        model.profile_change()
        self.assertFalse(model.have_sample)
        self.assertEqual(model.state, State.HOLDOFF)
        snap = model.process_sample(0, 0xFFFFFFF0, 500_000_000)
        self.assertEqual(snap["limited"], 0)
        self.assertEqual(snap["offset_ug"], -175_000)
        self.assertEqual(snap["corrected_gross_ug"], 500_175_000)
        snap = model.process_sample(2, 84, 500_000_000)
        self.assertEqual(snap["state"], State.LIMITED)
        self.assertEqual(snap["last_rebase_reason"], Reason.SEQUENCE)

    def test_dosing_profile_change_freezes_positive_offset(self):
        model = ReferenceLock()
        model.set_mode(Mode.DOSING_NO_COMPENSATION)
        model.offset_ug = 321_000
        model.offset_milli_ug = 321_000_000
        for sequence in range(4):
            model.profile_change()
            snap = model.process_sample(sequence * 1000,
                sequence * 100000, 500_000_000)
            self.assertEqual(snap["state"], State.DOSING)
            self.assertEqual(snap["offset_ug"], 321_000)
            self.assertEqual(snap["corrected_gross_ug"], 499_679_000)


class ReferenceLockReconstructionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        output = Path(cls.temp.name) / "report"
        subprocess.run([sys.executable, str(HERE / "evaluate_reference_lock.py"),
                        "--output", str(output)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        cls.report = json.loads((output / "offline_report.json").read_text())

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_frozen_r4_summary_reproduced(self):
        report = self.report
        self.assertEqual(report["run_count"], 6)
        self.assertAlmostEqual(report["median_ols_improvement_fraction"], 0.0994, delta=0.001)
        self.assertAlmostEqual(report["median_endpoint_improvement_fraction"], 0.1969, delta=0.001)
        self.assertAlmostEqual(report["minimum_endpoint_improvement_fraction"], 0.0437, delta=0.001)
        self.assertEqual(report["formal_reverse_amplification_count"], 0)
        self.assertLessEqual(report["maximum_10s_offset_change_g"], 0.0005)

    def test_adverse_loaded_run_is_preserved(self):
        run = next(item for item in self.report["runs"] if item["name"] == "loaded_500g_2")
        self.assertAlmostEqual(run["raw_ols_g_per_h"], -0.01938, delta=0.00001)
        self.assertAlmostEqual(run["corrected_ols_g_per_h"], -0.02132, delta=0.00002)
        self.assertLess(run["ols_improvement_fraction"], 0)
        self.assertGreater(run["endpoint_improvement_fraction"], 0.04)

    def test_synthetic_and_dosing_protection(self):
        synthetic = self.report["synthetic_12h"]
        self.assertEqual(synthetic["classification"], "SYNTHETIC_NOT_HARDWARE_VALIDATION")
        self.assertAlmostEqual(synthetic["final_offset_g"], 0.203054, delta=0.00001)
        self.assertEqual(synthetic["loaded_display_g_at_e_0_05"], 500.0)
        self.assertEqual(synthetic["unloaded_display_g_at_e_0_05"], 0.0)
        protection = self.report["protection"]
        self.assertEqual(protection["automatic_rebase_count"], 10)
        self.assertEqual(protection["dosing_offset_change_g"], 0)
        self.assertEqual(protection["slow_case_failures"], [])


if __name__ == "__main__":
    unittest.main()
