import unittest
from r3_raw_closure import decode,i32,i64,u32

class DecodeTests(unittest.TestCase):
    def test_signed_32_offsets_and_signed_64_weights(self):
        w=[0]*40
        w[14:16]=[65535,65535];w[38:40]=[65535,65535]
        w[18:22]=[65535]*4;w[22:26]=[0]*4
        s=decode(w)
        self.assertEqual(s['applied_offset_ug'],-1)
        self.assertEqual(s['candidate_offset_ug'],-1)
        self.assertEqual(s['uncompensated_gross_ug'],-1)
        self.assertEqual(s['gross_ug'],0)
    def test_same_block_field_mapping(self):
        w=[0]*40
        def put(i,n):w[i:i+2]=[n>>16,n&65535]
        put(0,0xA13E0001);put(2,5);put(8,1);put(10,123)
        put(14,45);put(16,1);put(20,500000000);put(24,499999955)
        put(28,499999955);put(34,1);put(36,1);put(38,45)
        s=decode(w)
        self.assertEqual(s['sequence'],123)
        self.assertEqual(s['generation'],5)
        self.assertEqual(s['gross_ug'],s['uncompensated_gross_ug']-s['applied_offset_ug'])
        self.assertEqual(s['net_ug'],s['gross_ug']-s['tare_ug'])
        self.assertEqual((s['mode'],s['state']),(1,1))
    def test_wrong_size_rejected(self):
        with self.assertRaisesRegex(ValueError,'length'):decode([0]*38)

if __name__=='__main__':unittest.main()
