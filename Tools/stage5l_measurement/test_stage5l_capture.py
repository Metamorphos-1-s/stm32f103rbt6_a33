import csv, json, tempfile, unittest
from pathlib import Path
import stage5l_capture as s

class Tests(unittest.TestCase):
    def test_word_decoders_and_wrap(self):
        self.assertEqual(-1,s.i32([0xffff,0xffff])); self.assertEqual(-1,s.i64([0xffff]*4)); self.assertEqual(0xffffffff,s.u32([0xffff,0xffff]))
        self.assertEqual(0,s.sequence_gap(0xffffffff,0)); self.assertEqual(2,s.sequence_gap(10,13))
    def test_empty_analysis_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d); (p/'samples.csv').write_text('mcu_uptime_ms,stable,overrun_count\n',encoding='utf-8')
            with self.assertRaises(ValueError): s.analyze_dir(p)
    def test_analysis_and_manifest(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d); keys=['mcu_uptime_ms','stable','overrun_count','raw_adc','filtered_raw','uncompensated_gross_ug','gross_ug','net_ug','conditioned_display_mass_ug','display_count']
            with (p/'samples.csv').open('w',newline='',encoding='utf-8') as f:
                w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerow(dict.fromkeys(keys,0));r=dict.fromkeys(keys,10);r['mcu_uptime_ms']=1000;r['stable']=1;r['overrun_count']=0;w.writerow(r)
            (p/'events.jsonl').write_text(json.dumps({'event':'EMPTY'})+'\n',encoding='utf-8');s.atomic_json(p/'environment.json',{});s.atomic_json(p/'summary.json',{})
            a=s.analyze_dir(p);self.assertEqual(2,a['records']);s.build_manifest(p);self.assertEqual(0,s.validate(p))
            (p/'samples.csv').write_text('tampered',encoding='utf-8')
            with self.assertRaises(ValueError): s.validate(p)
    def test_truncated_csv_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'samples.csv').write_text('raw_adc\n1\n',encoding='utf-8')
            with self.assertRaises((KeyError,ValueError)): s.analyze_dir(p)
if __name__=='__main__': unittest.main()
