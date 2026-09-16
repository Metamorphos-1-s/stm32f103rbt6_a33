#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import stage5mr4_blind_replay as replay


class Stage5MR4BlindReplayTests(unittest.TestCase):
    def test_frozen_parameters_are_exact(self):
        self.assertEqual(replay.FROZEN.observation_window_s, 180)
        self.assertEqual(replay.FROZEN.endpoint_median_s, 15)
        self.assertEqual(replay.FROZEN.estimator_deadband_ug, 2000)
        self.assertEqual(replay.FROZEN.max_static_rate_g_per_h, 1.0)
        self.assertEqual(replay.FROZEN.step_threshold_ug, 20000)
        self.assertEqual(replay.FROZEN.maximum_update_ug_per_s, 75)
        self.assertEqual(replay.FROZEN.correction_gain_permille, 875)
        self.assertEqual(replay.FROZEN.hold_off_s, 15)

    def test_mode_schedule_has_exactly_twenty_switches(self):
        self.assertEqual(len(replay.MODE_SWITCH_SECONDS), 20)
        self.assertEqual(len(set(replay.MODE_SWITCH_SECONDS)), 20)
        self.assertEqual(replay.mode_for_second(0), False)
        self.assertEqual(replay.mode_for_second(60), True)

    def test_all_required_holdouts_are_bound(self):
        self.assertEqual(len(replay.RUNS), 6)
        self.assertIn("20260916T_r4_1kg_constant", [run[1] for run in replay.RUNS])
        self.assertEqual(replay.CYCLE_RUN, "20260916T_r4_load_unload_cycles_5")


if __name__ == "__main__":
    unittest.main()
