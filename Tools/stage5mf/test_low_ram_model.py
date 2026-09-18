import unittest
from low_ram_model import Config, LowRamAdaptive, FAST, PRECISION


class LowRamModelTests(unittest.TestCase):
    def test_static_reaches_precision(self):
        model = LowRamAdaptive(Config())
        result = None
        for index in range(30):
            result = model.process(1000000, 1000000, index * 100, index + 1)
        self.assertEqual(result["state"], PRECISION)
        self.assertEqual(result["candidate_mass_ug"], 1000000)

    def test_dosing_never_stable_and_preserves_step(self):
        model = LowRamAdaptive(Config())
        for index in range(100):
            mass = 0 if index < 10 else 500000000
            result = model.process(mass, mass, index * 100, index + 1, True)
            self.assertEqual(result["state"], FAST)
            self.assertFalse(result["stable_candidate"])
        self.assertGreater(result["candidate_mass_ug"], 499999000)

    def test_reset_and_gap_are_bounded(self):
        model = LowRamAdaptive(Config())
        model.process(0, 0, 0, 1)
        result = model.process(1000, 500, 1000, 9)
        self.assertFalse(result["fault"])
        self.assertTrue(result["sample_gap_observed"])
        model.reset()
        self.assertFalse(model.initialized)


if __name__ == "__main__": unittest.main()
