import unittest

from serial_transport import SerialUnavailable, select_serial_port


def port(device, vid=None, hwid="", description=""):
    return {"device": device, "vid": vid, "hwid": hwid,
            "description": description}


class PortSelectionTests(unittest.TestCase):
    def test_no_usb_device(self):
        with self.assertRaisesRegex(SerialUnavailable, "NO SERIAL PORT FOUND"):
            select_serial_port("auto", [port("COM1")])

    def test_single_usb_device(self):
        self.assertEqual(select_serial_port("auto", [port("COM5", 0x1A86)]),
                         "COM5")

    def test_multiple_usb_devices_require_selection(self):
        ports = [port("COM5", 1), port("COM6", 2)]
        self.assertEqual(select_serial_port("auto", ports, lambda _: "2"),
                         "COM6")

    def test_explicit_port_is_unchanged(self):
        self.assertEqual(select_serial_port("COM9", []), "COM9")


if __name__ == "__main__":
    unittest.main()
