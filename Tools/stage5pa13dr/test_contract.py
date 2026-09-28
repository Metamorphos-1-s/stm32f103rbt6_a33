"""Simulate serial command latency and inject evidence failures; no device."""
import copy
import unittest
from contract import Checker, decode, SIGNATURE

SAFETY=dict(firmware=0x051f,map=0x0108,dirty=0,application=0,revision=19,
            saved_revision=19,save_count=0,calibration_valid=1)


def snapshot():
    return dict(signature=SIGNATURE,version=1,generation_begin=100,generation_end=100,
        flags=1,leased=1,control=0,touched=3000,static_end=1000,hclk_hz=72000000,
        fault=0,overrun=0,read_errors=0,invalid=0,counts=[1]+[0]*9,total=1,
        maxima=[1000]+[0]*9,peaks=[1]+[0]*9,coverage=1,first=1,last=1,
        max_cycles=1000,peak_seq=1,peak_path=0,loop_max=10000,interval_max=12000,
        produced=1,consumed=1,engine=1,fifo=0,now_ms=5000,driver_state=4,loops=50)


def after_command():
    s=snapshot()
    # Host spent 200ms issuing a mode command; sample2 holds the real peak.
    s.update(generation_begin=200,generation_end=200,total=3,last=3,now_ms=5200,
        produced=3,consumed=3,engine=3,max_cycles=600000,peak_seq=2,peak_path=4,
        coverage=17,loops=60)
    s['counts'][4]=2
    s['maxima'][4]=600000
    s['peaks'][4]=2
    return s


class ContractTests(unittest.TestCase):
    def test_exact_complete_atomic_block_layout(self):
        words=[0]*122
        def put(offset,value):words[offset:offset+2]=[value>>16,value&65535]
        put(0,SIGNATURE);put(2,200);put(4,72000000);put(6,(20<<8)|1)
        put(64,123456);words[84]=7;put(94,321);put(120,200)
        d=decode(words)
        self.assertEqual(d['maxima'][0],123456)
        self.assertEqual(d['counts'][0],7)
        self.assertEqual(d['peaks'][0],321)
        self.assertEqual(d['generation_begin'],d['generation_end'])
        self.assertEqual(d['overhead_cycles'],20)
    def test_per_path_peak_identity_not_mixed(self):
        s=after_command();s['peak_seq']=3
        with self.assertRaisesRegex(ValueError,'peak identity'):
            self.checker().check(s,5200000000,SAFETY)
    def checker(self):
        c=Checker(); c.check(snapshot(),5000000000,SAFETY); return c
    def test_serial_command_skips_poll_not_target_peak(self):
        c=self.checker(); s=after_command()
        c.check(s,5200000000,SAFETY)
        self.assertEqual(c.previous[0]['max_cycles'],600000)
        self.assertEqual(c.previous[0]['peak_seq'],2)
        self.assertEqual(c.gaps[0]['calls_not_seen_as_individual_polls'],1)
        self.assertEqual(c.complete(),'INCOMPLETE')  # missing paths, not fake PASS
    def test_timeout_in_unseen_call_fails(self):
        s=after_command(); s['maxima'][4]=720001; s['max_cycles']=720001
        with self.assertRaisesRegex(ValueError,'10ms'):
            self.checker().check(s,5200000000,SAFETY)
    def test_stack_breach_fails(self):
        s=after_command(); s['touched']=s['static_end']+511
        with self.assertRaisesRegex(ValueError,'stack'):
            self.checker().check(s,5200000000,SAFETY)
    def test_restart_fails_even_if_firmware_identity_same(self):
        s=after_command(); s['now_ms']=100
        with self.assertRaisesRegex(ValueError,'restart'):
            self.checker().check(s,5200000000,SAFETY)
    def test_target_loss_fails(self):
        s=after_command(); s['produced']=4
        with self.assertRaisesRegex(ValueError,'conservation'):
            self.checker().check(s,5200000000,SAFETY)
    def test_mixed_generation_fails(self):
        s=after_command(); s['generation_end']=202
        with self.assertRaisesRegex(ValueError,'atomic'):
            self.checker().check(s,5200000000,SAFETY)
    def test_missed_target_invocation_fails(self):
        s=after_command(); s['counts'][4]=1; s['total']=2
        with self.assertRaisesRegex(ValueError,'sample loss'):
            self.checker().check(s,5200000000,SAFETY)
    def test_heartbeat_not_disguised_as_continuous(self):
        with self.assertRaisesRegex(ValueError,'INCOMPLETE'):
            self.checker().check(after_command(),11000000000,SAFETY)


if __name__=='__main__':
    unittest.main()
