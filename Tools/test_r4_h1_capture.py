import json
import tempfile
import unittest
from pathlib import Path
from r4_h1_capture import parse_fc03,next_token,decode,validate_atomic,Consecutive,unique_directory
from modbus_frame import append_crc,build_read

def words(offset=-45,mode=2):
    w=[0]*40
    def put32(i,n):n&=0xffffffff;w[i:i+2]=[n>>16,n&65535]
    def put64(i,n):n&=(1<<64)-1;put32(i,n>>32);put32(i+2,n)
    put32(0,0xA13E0001);put32(2,7);put32(8,1);put32(10,91);put32(12,9100)
    put32(14,offset);put32(16,1);put64(18,500000000);put64(22,500000000-offset)
    put64(26,500000000-offset-100000000);put64(30,100000000)
    put32(34,mode);put32(36,7 if mode==2 else 1);put32(38,offset)
    return w

class Tests(unittest.TestCase):
    def test_signed_raw_fc03_exact_equations(self):
        tx=build_read(1,0x340,40)
        for offset in (-500000,-45,0,45,500000):
            w=words(offset);rx=append_crc(bytes([1,3,80])+b''.join(v.to_bytes(2,'big') for v in w))
            parsed=parse_fc03(tx,rx);a=parsed['atomic']
            self.assertEqual(a['applied_offset_ug'],offset)
            self.assertEqual(a['candidate_offset_ug'],offset)
            self.assertEqual(validate_atomic(a),(0,0))
            self.assertEqual(parsed['words'],w)
    def test_crc_failure_excluded(self):
        w=words();rx=bytearray(append_crc(bytes([1,3,80])+b''.join(v.to_bytes(2,'big') for v in w)))
        rx[10]^=1
        with self.assertRaisesRegex(ValueError,'CRC'):parse_fc03(build_read(1,0x340,40),rx)
    def test_numerical_failure_not_pass(self):
        a=decode(words());a['gross_ug']+=1
        with self.assertRaisesRegex(ValueError,'equation'):validate_atomic(a)
    def test_new_token_wrap_does_not_reuse_zero(self):
        self.assertEqual(next_token(65535),1)
        self.assertEqual(next_token(5001),5002)
    def test_existing_directory_cannot_be_overwritten(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'evidence';unique_directory(p);f=p/'samples.jsonl';f.write_text('original')
            with self.assertRaises(FileExistsError):unique_directory(p)
            self.assertEqual(f.read_text(),'original')
    def test_duplicates_and_gaps_not_interpolated(self):
        c=Consecutive()
        self.assertEqual(c.feed(1,True),1);self.assertEqual(c.feed(1,True),1)
        self.assertEqual(c.feed(2,True),2);self.assertEqual(c.feed(4,True),1)
        self.assertEqual(c.gaps,[dict(previous=2,current=4,missing=1)])
        self.assertEqual(c.feed(5,False),0)
        with self.assertRaisesRegex(ValueError,'regressed'):c.feed(1,True)

if __name__=='__main__':unittest.main()
