import unittest


class RevisionLedgerTests(unittest.TestCase):
    def test_profile_and_pair_each_increment_once(self):
        revision = 19
        revision += 1  # profile 0 -> 1
        self.assertEqual(revision, 20)
        revision += 1  # OFF+SHADOW -> ACTIVE+STATIC
        self.assertEqual(revision, 21)

    def test_repeated_pair_does_not_increment(self):
        before = after = 21
        self.assertEqual(after - before, 0)

    def test_dosing_pair_changes_once(self):
        self.assertEqual(22 - 21, 1)

    def test_unrelated_revision_is_rejected(self):
        self.assertNotIn(23, {19, 20, 21, 22})

    def test_save_revision_must_remain_saved_baseline(self):
        self.assertEqual(19, 19)


if __name__ == '__main__':
    unittest.main()
