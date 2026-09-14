#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import stage5mr2_threshold_replay as model

class Stage5MR2Tests(unittest.TestCase):
    def test_flat_blocks_count_against_direction_consistency(self):
        blocks = [0, 10, 10, 10, 10]
        self.assertEqual(model.direction_consistency(blocks, 1), 0.25)

    def test_partial_final_block_is_excluded(self):
        points = [(second, 0, 0) for second in range(60)]
        points.append((60, 0, 1_000_000))
        metrics = model.block_metrics(points, 2)
        self.assertEqual(metrics["blocks"], 1)
        self.assertEqual(metrics["endpoint_g"], 0)

    def test_disabled_mode_preserves_existing_offset(self):
        samples = [(second, second * 1000, False) for second in range(120)]
        state, output, _ = model.replay(samples, initial_offset_ug=5000)
        self.assertEqual(state.offset, 5000)
        self.assertTrue(all(row[3] == 5000 for row in output))

    def test_reenable_rebuilds_reference_without_chasing_load(self):
        samples = []
        mass = 0
        for second in range(1201):
            enabled = second > 120
            if not enabled and second and second % 10 == 0:
                mass += 100
            samples.append((second, mass, enabled))
        state, _, _ = model.replay(samples)
        self.assertEqual(state.offset, 0)
        self.assertEqual(state.updates, 0)

    def test_complete_offline_gate(self):
        report = model.build_report()
        self.assertEqual(report["status"], "PRELIMINARY_OFFLINE_CANDIDATE_MODE_GATED")
        self.assertTrue(all(report["gates"].values()))
        self.assertFalse(report["product_integration_authorized"])
        self.assertFalse(report["hardware_shadow_authorized"])

if __name__ == "__main__":
    unittest.main()
