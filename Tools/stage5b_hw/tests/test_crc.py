import unittest

import modbus_crc


class CrcTests(unittest.TestCase):
    def test_check_vector(self):
        self.assertEqual(modbus_crc.calculate(b"123456789"), 0x4B37)

    def test_wire_order_and_verify(self):
        framed = modbus_crc.append(b"123456789")
        self.assertEqual(framed[-2:], b"\x37\x4b")
        self.assertTrue(modbus_crc.verify(framed))

    def test_segmented_update(self):
        crc = modbus_crc.update(0xFFFF, b"1234")
        crc = modbus_crc.update(crc, b"56789")
        self.assertEqual(crc, 0x4B37)


if __name__ == "__main__": unittest.main()
