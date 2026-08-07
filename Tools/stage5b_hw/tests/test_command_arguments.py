import unittest

import register_map as reg
from stage5b_hw import parser
from test_commands import validate_arguments


class CommandArgumentTests(unittest.TestCase):
    def test_command_25_and_cli_arguments(self):
        self.assertEqual(reg.COMMANDS["RUNTIME_DRIFT_CONTROL"], 25)
        args = parser().parse_args(["commands", "--name", "RUNTIME_DRIFT_CONTROL",
                                    "--arg0", "0x1", "--arg1", "2"])
        self.assertEqual((args.arg0, args.arg1), (1, 2))

    def test_runtime_drift_rejects_arg0_two(self):
        with self.assertRaises(ValueError):
            validate_arguments("RUNTIME_DRIFT_CONTROL", 2, 0)

    def test_unsigned_32_bit_range(self):
        with self.assertRaises(ValueError):
            validate_arguments("NOP", -1, 0)
        with self.assertRaises(ValueError):
            validate_arguments("NOP", 0, 0x100000000)


if __name__ == "__main__":
    unittest.main()
