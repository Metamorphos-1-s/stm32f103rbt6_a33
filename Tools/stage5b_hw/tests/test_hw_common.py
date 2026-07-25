import unittest

from hw_common import execute_command, HardwareTestError


class FakeClient:
    def __init__(self, token):
        self.token = token
        self.read_count = 0

    def write_multiple(self, _address, _values):
        pass

    def write_single(self, _address, _value):
        pass

    def read(self, _address, _quantity):
        self.read_count += 1
        if self.read_count == 1:
            raise HardwareTestError("Modbus response timeout")
        return [self.token, 0, 0, 13], None


class CommandTests(unittest.TestCase):
    def test_transient_response_timeout_is_retried(self):
        client = FakeClient(123)
        response = execute_command(client, 123, 13)
        self.assertEqual(response["result_name"], "OK")
        self.assertEqual(client.read_count, 2)


if __name__ == "__main__":
    unittest.main()
