"""Synthetic boundary tests; no hardware is accessed."""

import unittest

from analyze_hot_cycles import phase, replay_r5, window


class HotCycleTests(unittest.TestCase):
    def test_checkpoint_requires_full_minute(self):
        rows = [{"host_monotonic_ns": str(s*1_000_000_000),
                 "gross_ug": str(500_000_000+s),
                 "raw_adc": "-480000", "filtered_raw": "-480000",
                 "display_count": "50000", "conditioned_display_ug": "500000000",
                 "display_anchor_ug": "500000000"} for s in range(75)]
        self.assertIn("NOT RUN", phase(rows, 0, 74*1_000_000_000+1, 0)["checkpoints"]["1"]["status"])

    def test_window_excludes_boundary(self):
        rows = [{"host_monotonic_ns": str(s*1_000_000_000)} for s in (15, 44, 45)]
        self.assertEqual(len(window(rows, 0, 15, 45)), 2)

    def test_counterfactual_retains_offset_across_modes(self):
        rows = []
        for s in range(2000):
            mass = 500_000_000 if s < 1000 or s >= 1500 else 0
            rows.append({"host_monotonic_ns": str(s*1_000_000_000),
                         "gross_ug": str(mass)})
        edges = {"unload1": {"edge_center_monotonic_ns": 1000*1_000_000_000},
                 "load2": {"edge_center_monotonic_ns": 1500*1_000_000_000},
                 "unload2": {"edge_center_monotonic_ns": 1800*1_000_000_000}}
        result = replay_r5(rows, edges)
        self.assertEqual(result["reset_count"], 0)
        self.assertEqual(result["max_dosing_offset_change_ug"], 0)
        for event in result["mode_switches"][1:]:
            self.assertEqual(event["offset_before_ug"], event["offset_after_ug"])


if __name__ == "__main__":
    unittest.main()
