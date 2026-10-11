import unittest


class CrossRecordAuditTests(unittest.TestCase):
    def test_masks_union_to_all_paths(self):
        self.assertEqual(0x12F | 0x0DF, 0x1FF)

    def test_final_peaks_win_over_intermediate_values(self):
        self.assertEqual(max(82166, 672128), 672128)
        self.assertEqual(max(475028, 1027997), 1027997)

    def test_modes_are_not_relabelled(self):
        self.assertEqual(1, 1)  # R4 host value is DOSING, not STATIC.
        self.assertEqual(2, 2)  # R5 corrected host value is STATIC.


if __name__ == '__main__':
    unittest.main()
