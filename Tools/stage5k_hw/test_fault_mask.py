import unittest
from fault_mask import decode

class FaultMaskTests(unittest.TestCase):
    def test_cs1237_data_error(self):
        self.assertEqual(decode([0, 0x20]), (0x20, ["CS1237_DATA_ERROR"]))

    def test_calibration_invalid(self):
        self.assertEqual(decode([0, 0x40]), (0x40, ["CALIBRATION_INVALID"]))

    def test_high_word_ui_state_error(self):
        self.assertEqual(decode([0x0040, 0]), (0x00400000, ["UI_STATE_ERROR"]))
