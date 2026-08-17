import unittest
from unittest import mock

import register_map as reg
from stage5b_hw import parser
from test_commands import validate_arguments
import test_commands


class CommandArgumentTests(unittest.TestCase):
    def test_command_25_and_cli_arguments(self):
        self.assertEqual(reg.COMMANDS["RUNTIME_DRIFT_CONTROL"], 25)
        args = parser().parse_args(["commands", "--name", "RUNTIME_DRIFT_CONTROL",
                                    "--arg0", "0x1", "--arg1", "2"])
        self.assertEqual((args.arg0, args.arg1), (1, 2))

    def test_runtime_drift_rejects_arg0_two(self):
        with self.assertRaises(ValueError):
            validate_arguments("RUNTIME_DRIFT_CONTROL", 2, 0)

    def test_explicit_runtime_drift_commands(self):
        self.assertEqual(reg.COMMANDS["RUNTIME_DRIFT_ENABLE"], 26)
        self.assertEqual(reg.COMMANDS["RUNTIME_DRIFT_DISABLE"], 27)
        self.assertEqual(reg.COMMANDS["RUNTIME_DRIFT_RESET"], 28)
        validate_arguments("RUNTIME_DRIFT_RESET", 0, 0)
        with self.assertRaises(ValueError):
            validate_arguments("RUNTIME_DRIFT_ENABLE", 1, 0)

    def test_unsigned_32_bit_range(self):
        with self.assertRaises(ValueError):
            validate_arguments("NOP", -1, 0)
        with self.assertRaises(ValueError):
            validate_arguments("NOP", 0, 0x100000000)

    def test_runtime_arguments_reach_mailbox_helper(self):
        response = {"result": 0, "result_name": "OK"}
        report = mock.Mock()
        with mock.patch("test_commands.execute_command", return_value=response) as execute:
            self.assertTrue(test_commands.run(object(), report,
                                              "RUNTIME_DRIFT_CONTROL", False, 1, 7))
        self.assertEqual(execute.call_args.args[2], 25)
        self.assertEqual(execute.call_args.args[3:5], (1, 7))


if __name__ == "__main__":
    unittest.main()
