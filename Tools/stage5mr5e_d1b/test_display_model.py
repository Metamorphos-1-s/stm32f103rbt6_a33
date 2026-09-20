import unittest
from display_model import CandidateConfig, IncrementalDisplay, quantize_count


class DisplayModelTests(unittest.TestCase):
    def test_symmetric_boundaries(self):
        self.assertEqual(quantize_count(4999, 1000000, 100, 1), 0)
        self.assertEqual(quantize_count(5000, 1000000, 100, 1), 1)
        self.assertEqual(quantize_count(-4999, 1000000, 100, 1), 0)
        self.assertEqual(quantize_count(-5000, 1000000, 100, 1), -1)

    def test_large_step_immediate(self):
        model = IncrementalDisplay(CandidateConfig(1, 2000))
        model.process(0, 0, True, True)
        result = model.process(50000, 100, False, True)
        self.assertEqual(result["actual_display_count"], 50000)
        self.assertTrue(result["large_step"])

    def test_slow_change_one_division_per_update(self):
        model = IncrementalDisplay(CandidateConfig(1, 1000))
        model.process(0, 0, True, True)
        model.process(3, 100, True, True)
        self.assertEqual(model.process(3, 1100, True, True)["actual_display_count"], 1)
        self.assertEqual(model.process(3, 1200, True, True)["actual_display_count"], 2)
        self.assertEqual(model.process(3, 1300, True, True)["actual_display_count"], 3)

    def test_off_shadow_passthrough(self):
        model = IncrementalDisplay(CandidateConfig(1, 1000))
        for index in range(20):
            result = model.process(index, index * 100, True, False)
            self.assertEqual(result["actual_display_count"], index)


if __name__ == "__main__": unittest.main()
