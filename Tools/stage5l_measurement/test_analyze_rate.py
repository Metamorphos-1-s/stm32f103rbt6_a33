import csv,tempfile,unittest
from pathlib import Path
import analyze_rate as a

class RateAnalysisTests(unittest.TestCase):
    def write(self,path,rows):
        with path.open('w',newline='',encoding='utf-8') as f:
            w=csv.DictWriter(f,fieldnames=rows[0],lineterminator='\n');w.writeheader();w.writerows(rows)
    def test_wrap_rate_robust_and_anomaly_metrics(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'s.csv';self.write(p,[{'sample_sequence':0xfffffffe,'mcu_uptime_ms':0xffffff00,'raw_adc':-10,'overrun_count':3},{'sample_sequence':0xffffffff,'mcu_uptime_ms':0xffffff64,'raw_adc':-11,'overrun_count':3},{'sample_sequence':1,'mcu_uptime_ms':44,'raw_adc':-0x700001,'overrun_count':4}]);r=a.analyze(p);self.assertEqual(3,r['sequence_delta']);self.assertEqual(300,r['mcu_timestamp_delta_ms']);self.assertEqual(2,r['maximum_sequence_jump']);self.assertEqual(1,r['near_rail_events']);self.assertEqual(1,r['single_sample_jump_events']);self.assertEqual(1,r['fifo_overrun_delta']);self.assertEqual(-11,r['raw_median']);self.assertEqual(1,r['raw_mad'])
    def test_empty_and_truncated_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'s.csv';p.write_text('sample_sequence,mcu_uptime_ms,raw_adc\n',encoding='utf-8');
            with self.assertRaises(ValueError):a.analyze(p)
if __name__=='__main__':unittest.main()
