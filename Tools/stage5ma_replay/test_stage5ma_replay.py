import csv,json,tempfile,unittest
from pathlib import Path
import stage5ma_replay as r

class ReplayTests(unittest.TestCase):
    def test_constant_and_determinism(self):
        a=r.Adaptive();b=r.Adaptive();x=[];y=[]
        for i in range(50):x.append(a.process(1000,i*100,i+1));y.append(b.process(1000,i*100,i+1))
        self.assertEqual(x,y);self.assertEqual(x[-1]["state"],0);self.assertEqual(x[-1]["stable_candidate"],1)
    def test_step_disturbance_then_transient(self):
        a=r.Adaptive()
        for i in range(20):a.process(0,i*100,i+1)
        self.assertEqual(a.process(500000000,2000,21)["state"],4);self.assertEqual(a.process(500000000,2100,22)["state"],1)
    def test_slow_change_not_stable(self):
        a=r.Adaptive();o=None
        for i in range(50):o=a.process(i*5000,i*100,i+1)
        self.assertEqual(o["state"],3);self.assertFalse(o["stable_candidate"])
    def test_gap_wrap_and_no_interpolation(self):
        a=r.Adaptive();a.process(0,0xfffffff0,0xffffffff);o=a.process(1,0x54,0);self.assertFalse(o["sample_gap_observed"]);o=a.process(2,1000,4);self.assertTrue(o["sample_gap_observed"])
    def test_calibration_symmetry(self):
        self.assertEqual(r.calibrated(r.CAL["raw_zero"]),0);self.assertEqual(r.calibrated(r.CAL["raw_span"]),500000000)
    def test_truncated_missing_columns(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/"samples.csv").write_text("raw_adc\n1\n",encoding="utf-8")
            old=r.ROOT;r.ROOT=p.parent
            with self.assertRaises(ValueError):r.load_rows(p.name)
            r.ROOT=old
    def test_split_has_no_overlap(self):
        groups={k:{n for n,v in r.RUNS.items() if v[1]==k} for k in ("development","holdout","regression")};self.assertFalse(groups["development"]&groups["holdout"]);self.assertFalse(groups["development"]&groups["regression"])
    def test_json_writer_lf(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"x.json";r.write_json(p,{"x":1});self.assertNotIn(b"\r\n",p.read_bytes());self.assertTrue(p.read_bytes().endswith(b"\n"));json.loads(p.read_text())
if __name__=="__main__":unittest.main()
