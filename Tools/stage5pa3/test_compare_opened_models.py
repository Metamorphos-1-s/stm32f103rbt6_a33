import unittest

from compare_opened_models import UnifiedRobustReference


class UnifiedSafetyTests(unittest.TestCase):
    def test_dosing_freezes_offset_and_preserves_step(self):
        model = UnifiedRobustReference()
        model.offset = 215114
        before, held = model.process(1, 500000000, "DOSING")
        after, later = model.process(2, 0, "DOSING")
        self.assertEqual((held, later), (215114, 215114))
        self.assertEqual(before - after, 500000000)
        self.assertEqual(model.rebuilds, 0)

    def test_static_load_step_keeps_inherited_offset(self):
        model = UnifiedRobustReference()
        model.offset = 215114
        before, _ = model.process(0, 0)
        after, inherited = model.process(1, 500000000)
        self.assertEqual(after - before, 500000000)
        self.assertEqual(inherited, 215114)
        self.assertEqual(model.rebuilds, 1)

    def test_zero_and_calibration_reset(self):
        model = UnifiedRobustReference()
        model.offset = 12345
        model.reference = 500000000
        model.reset()
        self.assertEqual(model.offset, 0)
        self.assertIsNone(model.reference)

    def test_profile_switch_keeps_offset_and_holds(self):
        model = UnifiedRobustReference()
        model.offset = 12345
        model.profile_change(100, 500000000, rate_hz=40, filter_mode=1)
        self.assertEqual(model.offset, 12345)
        self.assertEqual(model.holdoff_until, 115)
        self.assertEqual(model.rebuilds, 1)
        self.assertEqual((model.rate_hz, model.filter_mode), (40, 1))
        model.profile_change(120, 500000000, rate_hz=10, filter_mode=3)
        self.assertEqual((model.rate_hz, model.filter_mode), (10, 3))
        with self.assertRaises(ValueError):
            model.profile_change(130, 500000000, rate_hz=640)

    def test_offset_rate_and_cap(self):
        model = UnifiedRobustReference()
        model.reference = 0
        model.window.extend([100000] * 60)
        model.previous_second = 100
        model.previous_mass = 100000
        for second in range(101, 111):
            model.process(second, 100000)
        self.assertLessEqual(model.offset, 500)
        model.offset = 499980
        model.window.clear()
        model.window.extend([1000000] * 60)
        model.previous_mass = 1000000
        model.process(111, 1000000)
        self.assertEqual(model.offset, 500000)


if __name__ == "__main__":
    unittest.main()
