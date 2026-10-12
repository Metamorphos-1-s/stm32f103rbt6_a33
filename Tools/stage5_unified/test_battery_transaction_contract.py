from battery_transaction import read_battery, target_words, words32


def test_target_words_and_signed_offset():
    assert target_words() == [0, 400, 0, 13600, 0, 13200, 0, 13900, 1]
    assert words32(-1) == [0xffff, 0xffff]


def test_active_decode_uses_one_atomic_block():
    class Client:
        def read(self, address, count):
            assert (address, count) == (0x03A0, 9)
            return ([0xffff, 0xff9c, 0, 13600, 0, 13200, 0, 13900, 1], None)
    assert read_battery(Client())["calibration_offset_mv"] == -100
    assert read_battery(Client())["low_voltage_alarm_enable"] == 1
