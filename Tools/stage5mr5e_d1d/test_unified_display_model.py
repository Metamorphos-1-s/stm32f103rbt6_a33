import unittest

from unified_display_model import UnifiedDisplay, UnifiedInput


class UnifiedDisplayModelTests(unittest.TestCase):
    def lock(self, model, division=1, source=8):
        for sequence in range(1, 11):
            model.process(UnifiedInput(0, sequence, (sequence - 1) * 100,
                source, 1, 2, division, True))
        self.assertTrue(model.locked)

    def test_ideal_follow_is_within_600_ms(self):
        model = UnifiedDisplay()
        self.lock(model)
        first_ms = None
        for index in range(1, 7):
            result = model.process(UnifiedInput(20000, 10 + index,
                900 + index * 100, 8, 1, 2, 1, True))
            if result["displayed"] == 1 and first_ms is None:
                first_ms = index * 100
        self.assertIsNotNone(first_ms)
        self.assertLessEqual(first_ms, 600)

    def test_exact_8d_is_slow_and_8d_plus_one_releases(self):
        model = UnifiedDisplay()
        self.lock(model)
        at_boundary = model.process(UnifiedInput(80000, 11, 1000,
            8, 1, 2, 1, True))
        self.assertFalse(at_boundary["large_step"])
        over_boundary = model.process(UnifiedInput(90000, 12, 1100,
            8, 1, 2, 1, True))
        self.assertTrue(over_boundary["large_step"])
        self.assertEqual(over_boundary["displayed"], 9)

    def test_duplicate_sequence_does_not_accumulate(self):
        model = UnifiedDisplay()
        self.lock(model)
        first = model.process(UnifiedInput(20000, 11, 1000,
            8, 1, 2, 1, True))
        repeated = model.process(UnifiedInput(20000, 11, 1020,
            8, 1, 2, 1, True))
        self.assertEqual(first["evidence"], repeated["evidence"])

    def test_all_r5_combinations_share_semantics(self):
        for source in (8, 9):
            for _mode in (0, 1, 2):
                model = UnifiedDisplay()
                self.lock(model, source=source)
                for index in range(5):
                    result = model.process(UnifiedInput(20000, 11 + index,
                        1000 + index * 100, source, 1, 2, 1, True))
                self.assertEqual(result["displayed"], 1)


if __name__ == "__main__":
    unittest.main()
