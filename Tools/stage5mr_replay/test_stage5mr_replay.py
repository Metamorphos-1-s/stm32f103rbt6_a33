import unittest
import stage5mr_replay as r
class T(unittest.TestCase):
 def test_off_bypass_math(self):self.assertEqual(r.delta(0xffffffff,0),1)
 def test_rate_budget(self):self.assertLessEqual(r.CFG['maximum_update_ug']*10,1000)
 def test_step_protected(self):self.assertLessEqual(r.synthetic(.005,10)['loss_g'],.005)
 def test_pause_scans(self):
  for t in(.5,1,2,5,10,15):self.assertTrue(r.synthetic(.02,t)['detected'])
 def test_deterministic(self):self.assertEqual(r.synthetic(.01,5),r.synthetic(.01,5))
if __name__=='__main__':unittest.main()
