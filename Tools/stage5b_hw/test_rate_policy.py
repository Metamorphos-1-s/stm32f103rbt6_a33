import unittest

from hw_common import HardwareTestError
from rate_policy import require_profile_authorization


class RatePolicyTests(unittest.TestCase):
    def test_profile_zero_allowed(self):
        require_profile_authorization(0)

    def test_profile_one_requires_engineering_authorization(self):
        with self.assertRaises(HardwareTestError):
            require_profile_authorization(1)
        require_profile_authorization(1, True)

    def test_unsupported_profiles_always_rejected(self):
        for profile in (-1, 2, 3, 4):
            with self.assertRaises(HardwareTestError):
                require_profile_authorization(profile, True)


if __name__ == "__main__":
    unittest.main()

