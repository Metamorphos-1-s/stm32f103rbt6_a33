import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import stage5na3_swd as swd


class Stage5NA3SwdTests(unittest.TestCase):
    def test_decode_control(self):
        words = [swd.MAGIC, swd.VERSION, swd.CONTROL_SIZE] + list(
            range(3, swd.CONTROL_WORDS))
        value = swd.decode_control(struct.pack("<%dI" % swd.CONTROL_WORDS,
                                               *words))
        self.assertEqual(value["magic"], swd.MAGIC)
        self.assertEqual(value["request_sequence"], 3)
        self.assertEqual(value["last_timestamp_ms"], 22)

    def test_decode_rejects_identity_and_truncation(self):
        with self.assertRaises(ValueError):
            swd.decode_control(bytes(swd.CONTROL_SIZE - 1))
        words = [0, swd.VERSION, swd.CONTROL_SIZE] + [0] * 20
        with self.assertRaises(ValueError):
            swd.decode_control(struct.pack("<23I", *words))

    @mock.patch("stage5na3_swd.subprocess.check_output")
    def test_symbol_contract(self, check_output):
        check_output.return_value = (
            "2000455c 0000005c B g_stage5na3_fault_control\n")
        value = swd.symbol_from_elf(Path("candidate.elf"), "nm")
        self.assertEqual(value, {"address": 0x2000455C, "size": 92})

    def test_writer_is_utf8_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "value.json"
            swd.write_json(path, {"value": 1})
            self.assertEqual(path.read_bytes(), b'{\n  "value": 1\n}\n')


if __name__ == "__main__":
    unittest.main()
