#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from r5_beta_hw import decode_beta


class R5BetaHardwareToolTests(unittest.TestCase):
    def test_decode_high_word_first(self):
        words = [0] * 40
        words[0] = 0x55B5; words[1] = 1; words[2] = 2; words[3] = 5
        words[6:10] = [0, 0, 0, 123]
        words[30:32] = [0xFFFF, 0xFFCE]
        words[32:35] = [15, 300, 600]
        value = decode_beta(words)
        self.assertEqual(value["application"], 1)
        self.assertEqual(value["offset_ug"], 123)
        self.assertEqual(value["correction_rate_milli_ug_per_s"], -50)
        self.assertEqual(value["observation_fill"], 600)

    def test_signature_required(self):
        with self.assertRaises(Exception): decode_beta([0] * 40)


if __name__ == "__main__": unittest.main()
