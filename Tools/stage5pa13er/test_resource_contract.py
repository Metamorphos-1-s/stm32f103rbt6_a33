"""No hardware: identity, ACTIVE and delayed target peak evidence."""
import sys
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/stage5pa13dr'))
from test_contract import snapshot as old_snapshot, after_command as old_after_command, SAFETY
sys.path.insert(0,str(Path(__file__).resolve().parent))
from resource_contract import Checker

def snapshot():
    return dict(old_snapshot(),mode=0)

def after_command():
    return dict(old_after_command(),mode=2)


class Contract(unittest.TestCase):
    def test_real_identity_and_active_peak_across_host_gap(self):
        safety=dict(SAFETY, firmware=0x0521,map=0x010a)
        check=Checker();check.check(snapshot(),5000000000,safety)
        delayed=after_command();delayed['mode']=2
        safety['application']=1
        check.check(delayed,5200000000,safety)
        self.assertEqual(check.gaps[0]['calls_not_seen_as_individual_polls'],1)
        self.assertEqual(check.previous[0]['max_cycles'],600000)
        self.assertEqual(check.previous[0]['peak_seq'],2)
    def test_previous_diagnostic_identity_refused(self):
        with self.assertRaisesRegex(ValueError,'wrong A13E'):
            Checker().check(snapshot(),5000000000,SAFETY)
    def test_invalid_application_refused(self):
        with self.assertRaisesRegex(ValueError,'invalid ACTIVE'):
            Checker().check(snapshot(),5000000000,
                dict(SAFETY,firmware=0x0521,map=0x010a,application=2))
    def test_timeout_and_stack_fault_preserved(self):
        safety=dict(SAFETY, firmware=0x0521,map=0x010a)
        a=Checker();a.check(snapshot(),5000000000,safety)
        b=after_command();b['maxima'][4]=720001;b['max_cycles']=720001
        with self.assertRaisesRegex(ValueError,'10ms'):
            a.check(b,5200000000,safety)
        b=after_command();b['touched']=b['static_end']+511
        with self.assertRaisesRegex(ValueError,'stack'):
            a.check(b,5200000000,safety)


if __name__=='__main__':unittest.main()
