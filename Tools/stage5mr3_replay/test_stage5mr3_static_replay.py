#!/usr/bin/env python3
import sys
import unittest
from dataclasses import replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import stage5mr3_static_replay as model


class Stage5MR3Tests(unittest.TestCase):
    def test_adc_normalization_is_sensor_scale_independent(self):
        for counts_per_g in (1000, 2000, 4000):
            raw_delta = 125 * counts_per_g
            self.assertEqual(
                model.normalize_adc_delta_to_ug(
                    raw_delta, counts_per_g, 1_000_000
                ),
                125_000_000,
            )

    def test_invalid_calibration_is_rejected(self):
        with self.assertRaises(ValueError):
            model.normalize_adc_delta_to_ug(10, 0, 500_000_000)

    def test_disabled_mode_freezes_offset(self):
        config = model.Config(observation_window_s=10, endpoint_median_s=2)
        samples = [(second, second * 1000, False) for second in range(100)]
        state, output, _ = model.replay(samples, config, 5000)
        self.assertEqual(state.offset, 5000)
        self.assertTrue(all(row[3] == 5000 for row in output))

    def test_small_static_drift_below_old_8mg_deadband_is_accepted(self):
        config = model.Config(
            observation_window_s=10,
            endpoint_median_s=2,
            estimator_deadband_ug=0,
            max_static_rate_g_per_h=1.0,
            maximum_update_ug_per_s=100,
        )
        samples = [(second, second * 100, True) for second in range(20)]
        state, _, _ = model.replay(samples, config)
        self.assertGreaterEqual(state.updates, 1)

    def test_cold2_class_rate_is_not_rejected(self):
        config = model.Config(
            observation_window_s=300,
            estimator_deadband_ug=500,
            max_static_rate_g_per_h=0.5,
        )
        # About 0.168 g/h, matching the fast static cold-start class.
        samples = [
            (second, round(second * 0.168 * 1_000_000 / 3600), True)
            for second in range(301)
        ]
        state, _, _ = model.replay(samples, config)
        self.assertEqual(state.updates, 1)
        self.assertEqual(
            state.decisions[-1]["reason"], "STATIC_DRIFT_RATE_ACCEPTED"
        )

    def test_fast_step_rebases_in_static_mode(self):
        config = model.Config(
            observation_window_s=30,
            endpoint_median_s=3,
            hold_off_s=5,
        )
        samples = []
        for second in range(80):
            mass = 0 if second < 20 else 100_000
            samples.append((second, mass, True))
        state, _, _ = model.replay(samples, config)
        self.assertGreaterEqual(state.step_events, 1)

    def test_grid_and_scaling_contract(self):
        self.assertEqual(len(model.candidate_grid()), 720)
        self.assertTrue(
            model.sensor_scaling_invariance(model.Config())["passed"]
        )


if __name__ == "__main__":
    unittest.main()
