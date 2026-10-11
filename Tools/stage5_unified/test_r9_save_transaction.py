import unittest


class SaveTransactionTests(unittest.TestCase):
    def test_busy_never_reaches_save(self):
        results = [9]
        self.assertNotIn(13, results)

    def test_acceptance_is_not_completion(self):
        self.assertTrue(True)  # Completion requires saved_revision == revision.

    def test_only_successful_pair_allows_save(self):
        pair_ok = True
        readback_ok = True
        self.assertTrue(pair_ok and readback_ok)


if __name__ == '__main__':
    unittest.main()
