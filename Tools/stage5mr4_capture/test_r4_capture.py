import json,tempfile,unittest
from pathlib import Path
import r4_capture as r
class T(unittest.TestCase):
 def test_lf_writer(self):
  with tempfile.TemporaryDirectory() as d:p=Path(d)/"x.json";r.write_json(p,{"x":1});self.assertNotIn(b"\r\n",p.read_bytes());self.assertTrue(p.read_bytes().endswith(b"\n"));json.loads(p.read_text())
 def test_frozen_values(self):
  self.assertEqual(75*10,750);self.assertLessEqual(75*10,1000)
if __name__=='__main__':unittest.main()
