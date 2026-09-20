import unittest

from directional_model import DirectionalConfig, DirectionalDisplay


CONFIG = DirectionalConfig(1, 1, 1, 5, False)


class DirectionalDisplayTests(unittest.TestCase):
    def test_adjacent_targets_preserve_directional_evidence(self):
        model = DirectionalDisplay(CONFIG)
        model.process(-8, True, True, baseline_count=-4)
        for desired in (-8, -7, -9, -6, -10):
            result = model.process(desired, True, True, baseline_count=-4)
        self.assertEqual(-5, result["current_count"])
        self.assertEqual(0, result["evidence"])

    def test_zero_leaks_evidence(self):
        model = DirectionalDisplay(CONFIG)
        model.process(0, True, True, baseline_count=0)
        model.process(1, True, True)
        self.assertEqual(1, model.evidence)
        model.process(0, True, True)
        self.assertEqual(0, model.evidence)

    def test_reversal_cancels_before_rebuilding(self):
        model = DirectionalDisplay(CONFIG)
        model.process(0, True, True, baseline_count=0)
        for _ in range(4):
            model.process(2, True, True)
        for _ in range(4):
            model.process(-2, True, True)
        self.assertEqual(0, model.evidence)
        self.assertEqual(0, model.display_count)

    def test_large_step_is_strictly_greater_than_eight(self):
        model = DirectionalDisplay(CONFIG)
        model.process(0, True, True, baseline_count=0)
        self.assertFalse(model.process(8, True, True)["large_step"])
        self.assertTrue(model.process(9, True, True)["large_step"])
        self.assertEqual(9, model.display_count)

    def test_negative_large_step_and_int_boundaries(self):
        model = DirectionalDisplay(CONFIG)
        model.process((1 << 31) - 1, True, True,
            baseline_count=(1 << 31) - 1)
        result = model.process(-(1 << 31), True, True)
        self.assertTrue(result["large_step"])
        self.assertEqual(-(1 << 31), model.display_count)

    def test_inactive_is_exact_baseline_passthrough(self):
        model = DirectionalDisplay(CONFIG)
        for count in range(-20, 21):
            result = model.process(count + 5, True, False,
                baseline_count=count)
            self.assertEqual(count, result["current_count"])

    def test_unit_or_display_source_change_resets_to_new_baseline(self):
        model = DirectionalDisplay(CONFIG)
        model.process(4, True, True, baseline_count=0, source=0x08)
        for _ in range(5):
            model.process(4, True, True, baseline_count=0, source=0x08)
        self.assertEqual(1, model.display_count)
        result = model.process(200, True, True, baseline_count=200,
            source=0x2D)
        self.assertEqual(200, result["current_count"])
        self.assertEqual(0, result["evidence"])

    def test_invalid_sample_requires_fresh_valid_initialization(self):
        model = DirectionalDisplay(CONFIG)
        model.process(100, True, True, valid=False, baseline_count=100)
        result = model.process(20, True, True, valid=True,
            baseline_count=20)
        self.assertEqual(20, result["current_count"])
        self.assertEqual(0, result["evidence"])


if __name__ == "__main__":
    unittest.main()
