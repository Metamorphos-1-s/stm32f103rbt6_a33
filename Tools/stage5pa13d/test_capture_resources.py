"""Host-only decoder tests; fake frames cannot count as hardware evidence."""
import unittest
from capture_resources import read


class FakeClient:
    def __init__(self):
        self.calls = []
        r = [0] * 32
        r[4] = 8
        r[14:16] = [0x0107, 0x051e]
        d = [0] * 28
        d[11] = 4
        d[19:23] = [0, 19, 0, 19]
        d[27] = 1
        beta = [0] * 40
        full = [0] * 112
        full[0] = 0xa13c
        full[17:21] = [0, 4, 0, 400]
        metrics = [72000000, 0, 120, 4, 0, 0, 0, 1000, 1100, 10000,
                   0x20004ab8, 25601, 0x200047b8, 0x20005000, 0, 4, 4, 0, 0, 0, 0, 4, 400, 4]
        for index, value in enumerate(metrics):
            full[64 + index * 2] = value >> 16
            full[65 + index * 2] = value & 65535
        self.blocks = {(0,32):r, (32,28):d, (448,10):[3]+[0]*9,
                       (640,40):beta, (768,112):full}

    def read(self, first, count):
        self.calls.append((first, count))
        return self.blocks[(first, count)], None


class DecoderTests(unittest.TestCase):
    def test_five_blocks_and_exact_fields(self):
        client = FakeClient()
        sample = read(client)
        self.assertEqual(client.calls, [(0,32), (32,28), (448,10), (640,40), (768,112)])
        self.assertEqual(sample['hclk_hz'], 72000000)
        self.assertEqual(sample['flags'], 1)
        self.assertEqual(sample['overhead_max_cycles'], 100)
        self.assertEqual(sample['call_cycles'], 120)
        self.assertEqual(sample['lowest_touched'] - sample['static_end'], 768)
        self.assertEqual(sample['produced'] - sample['consumed'], sample['fifo'])

    def test_mismatch_not_silently_mapped_to_same_sample(self):
        client = FakeClient()
        client.blocks[(768,112)][18] = 5
        sample = read(client)
        self.assertEqual(sample['candidate_sequence'], 5)
        self.assertEqual(sample['sequence'], 4)
        self.assertEqual(sample['call_sequence'], 4)


if __name__ == '__main__':
    unittest.main()
