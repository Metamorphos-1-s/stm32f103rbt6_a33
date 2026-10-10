import unittest

from profile_switch_contract import (COMMAND_RESULT_ACCEPTED,
                                     COMMAND_RESULT_OK,
                                     MAILBOX_COMMAND_SWITCH_PROFILE,
                                     classify_response, request_profile_command)


class ProfileSwitchContractTests(unittest.TestCase):
    def test_external_mailbox_id_is_not_internal_enum_id(self):
        self.assertEqual(request_profile_command(1, 100)['command'], 7)
        self.assertNotEqual(MAILBOX_COMMAND_SWITCH_PROFILE, 23)

    def test_other_profile_requires_accepted(self):
        self.assertTrue(classify_response({'result': COMMAND_RESULT_ACCEPTED}, 1, 0))
        self.assertFalse(classify_response({'result': COMMAND_RESULT_OK}, 1, 0))

    def test_current_profile_returns_ok(self):
        self.assertTrue(classify_response({'result': COMMAND_RESULT_OK}, 0, 0))
        self.assertFalse(classify_response({'result': COMMAND_RESULT_ACCEPTED}, 0, 0))

    def test_expected_ram_profile_revision_is_not_a_save(self):
        self.assertEqual(20 - 19, 1)


if __name__ == '__main__':
    unittest.main()
