import unittest

from checkweigh_model import *


class CheckweighShadowTests(unittest.TestCase):
    def model(self):
        return CheckweighShadow(ShadowConfig(2, 10000, 2, 0))

    def step(self, model, index, static=150000000, dynamic=150000000,
             **kwargs):
        stable = kwargs.pop("stable", True)
        process_active = kwargs.pop("process_active", False)
        return model.process(sequence=index, timestamp_ms=index * 100,
            static_weight_ug=static, dynamic_weight_ug=dynamic,
            low_limit_ug=100000000, high_limit_ug=200000000,
            stable=stable, process_active=process_active, **kwargs)

    def test_threshold_equalities_are_ok(self):
        self.assertEqual(OK, classify(100, 100, 200))
        self.assertEqual(OK, classify(200, 100, 200))
        self.assertEqual(LOW, classify(99, 100, 200))
        self.assertEqual(HIGH, classify(201, 100, 200))

    def test_invalid_limits(self):
        model = self.model()
        row = model.process(sequence=1, timestamp_ms=100,
            static_weight_ug=0, dynamic_weight_ug=0, low_limit_ug=2,
            high_limit_ug=1, stable=True, process_active=False)
        self.assertEqual((INVALID, INVALID),
            (row["static_class"], row["dynamic_confirmed"]))

    def test_static_confirmation_and_dosing_suppression(self):
        model = self.model()
        self.assertEqual(PENDING, self.step(model, 1)["static_class"])
        self.assertEqual(OK, self.step(model, 2)["static_class"])
        row = self.step(model, 3, process_active=True)
        self.assertEqual(PENDING, row["static_class"])
        self.assertEqual(SUPPRESS_PROCESS_ACTIVE, row["static_reason"])

    def test_dynamic_confirmation_and_hysteresis(self):
        model = self.model()
        self.step(model, 1, dynamic=50000000)
        row = self.step(model, 2, dynamic=50000000)
        self.assertEqual(LOW, row["dynamic_confirmed"])
        for index, mass in ((3, 100000000), (4, 109999999)):
            row = self.step(model, index, dynamic=mass)
        self.assertEqual(LOW, row["dynamic_confirmed"])
        self.step(model, 5, dynamic=110000000)
        row = self.step(model, 6, dynamic=110000000)
        self.assertEqual(OK, row["dynamic_confirmed"])

    def test_fault_overload_calibration_never_valid(self):
        for key in ("fault", "overload", "calibration"):
            model = self.model(); kwargs = {key: True}
            row = self.step(model, 1, **kwargs)
            self.assertEqual(INVALID, row["static_class"])
            self.assertEqual(INVALID, row["dynamic_confirmed"])

    def test_sequence_and_timestamp_reset(self):
        model = self.model(); self.step(model, 1); self.step(model, 2)
        row = self.step(model, 4)
        self.assertEqual(SUPPRESS_SEQUENCE, row["static_reason"])
        row = model.process(sequence=5, timestamp_ms=900,
            static_weight_ug=0, dynamic_weight_ug=0,
            low_limit_ug=-1, high_limit_ug=1, stable=True,
            process_active=False)
        self.assertEqual(SUPPRESS_TIMESTAMP, row["dynamic_reason"])

    def test_calibration_rounding_and_direction(self):
        self.assertEqual(0, calibrate_raw(-44047))
        self.assertEqual(500000000, calibrate_raw(-487965))
        self.assertGreater(calibrate_raw(-100000), 0)


if __name__ == "__main__":
    unittest.main()
