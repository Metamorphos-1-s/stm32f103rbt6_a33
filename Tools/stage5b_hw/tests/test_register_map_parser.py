import unittest
from register_map_check import check


class MapTests(unittest.TestCase):
    def test_current_source(self): self.assertTrue(check())


if __name__ == "__main__": unittest.main()
