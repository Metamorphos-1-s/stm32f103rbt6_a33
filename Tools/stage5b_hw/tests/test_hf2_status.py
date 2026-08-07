import unittest

import register_map as reg
from hf2_status import parse_status


def words(value, count, order="high"):
    value &= (1 << (count * 16)) - 1
    result = [(value >> ((count - index - 1) * 16)) & 0xFFFF
              for index in range(count)]
    return result if order == "high" else list(reversed(result))


class Hf2StatusTests(unittest.TestCase):
    def make_blocks(self, order="high"):
        realtime = [0] * 0x20
        diagnostic = [0] * 0x1B
        active = [0] * 0x40
        display = [0] * 0x11
        drift = [0] * 0x1E
        realtime[4:6] = words(reg.STATUS_STABLE | reg.STATUS_TARE_ACTIVE, 2, order)
        realtime[0x10:0x14] = words(-123456, 4, order)
        realtime[0x14:0x18] = words(500000000, 4, order)
        realtime[0x1C:0x1E] = words(-100, 2, order)
        active[4:8] = words(3000000000, 4, order)
        display[2:6] = words(499999000, 4, order)
        drift[0] = 2
        drift[4:8] = words(-500, 4, order)
        drift[8:12] = words(500000000, 4, order)
        drift[24:26] = words(300000, 2, order)
        drift[28:30] = words(301, 2, order)
        identity = {"word_order": order, "register_map_version": 0x0102}
        return identity, realtime, diagnostic, active, display, drift

    def test_high_word_first_signed_and_whole_block(self):
        result = parse_status(*self.make_blocks("high"))
        self.assertEqual(result["runtime_drift"]["offset"]["ug"], -500)
        self.assertEqual(result["runtime_drift"]["state"]["name"], "TRACKING")
        self.assertEqual(result["runtime_drift"]["sample_count"], 301)
        self.assertEqual(result["weighing"]["net"]["ug"], -123456)

    def test_low_word_first_positive_and_negative(self):
        result = parse_status(*self.make_blocks("low"))
        self.assertEqual(result["weighing"]["gross"]["ug"], 500000000)
        self.assertEqual(result["runtime_drift"]["offset"]["ug"], -500)
        self.assertTrue(result["weighing"]["tare_active"])

    def test_reserved_is_exposed(self):
        blocks = list(self.make_blocks())
        blocks[-1][3] = 7
        self.assertEqual(parse_status(*blocks)["runtime_drift"]["reserved"], 7)


if __name__ == "__main__":
    unittest.main()
