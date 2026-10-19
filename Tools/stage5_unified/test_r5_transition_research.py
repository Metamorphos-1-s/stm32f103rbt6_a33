"""Counterexample and scoring contracts, no devices or subprocess runners."""
import unittest
import tempfile
from pathlib import Path

import research_r5_transition as study


class TransitionStudyTests(unittest.TestCase):
    def test_changed_core_source_cannot_be_reported_as_frozen_baseline(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            p = root / "Domain/measurement/a13c_shadow_compensator.c"
            p.parent.mkdir(parents=True)
            p.write_text("wrong baseline")
            with self.assertRaisesRegex(ValueError, "frozen source mismatch"):
                study.source_identity(root)

    def test_every_droplet_size_has_a_matched_static_and_dosing_control(self):
        specs = study.scenarios()
        for drop in (20_000, 50_000, 100_000):
            controls = [s for s in specs if s["kind"] == "drops" and s["drop_ug"] == drop]
            self.assertEqual({s["dosing"] for s in controls}, {True, False})

    def test_real_residue_and_mechanical_residual_have_identical_adc(self):
        for residual in (20_000, 40_000, 80_000):
            a = dict(kind="mechanical_residual", residual_ug=residual)
            b = dict(kind="real_residue", residual_ug=residual)
            for second in (0, 239.975, 240, 599.975, 600, 615, 1199.975):
                sequence = round(second * study.RATE) + 1
                left, right = study.sample(a, sequence), study.sample(b, sequence)
                self.assertEqual(left[:2], right[:2])
                self.assertEqual(right[2] - left[2], residual if second >= 600 else 0)

    def test_pause2_and_pause10_really_contain_no_new_drops(self):
        for second in (390, 390.5, 391, 391.975):
            self.assertEqual(study.drops_before(second), 30)
        for second in (422, 425, 430, 431.975):
            self.assertEqual(study.drops_before(second), 60)
        self.assertEqual(study.drops_before(392), 31)
        self.assertEqual(study.drops_before(432), 61)
        self.assertEqual(study.drops_before(1199), 90)

    def test_simulated_dosing_spans_both_pauses_and_does_not_exit_on_quiet(self):
        spec = dict(kind="drops", drop_ug=20_000, dosing=True)
        for second in (350, 391, 431, 479):
            self.assertEqual(study.sample(spec, second * 40 + 1)[1], 1)
        self.assertEqual(study.sample(spec, 480 * 40 + 1)[1], 2)

    def test_missing_physical_samples_are_refused(self):
        with self.assertRaisesRegex(ValueError, "omitted physical samples"):
            study.parse_output(b"sequence,net_ug\n1,0\n")

    def test_absolute_mass_error_cannot_be_ranked_as_a_zero_improvement(self):
        baseline = dict(return_windows={"late": dict(return_error_ug=0)})
        incorrect_zero = dict(return_windows={"late": dict(return_error_ug=-30_000)})
        result = {p: dict(metrics=baseline if p == "baseline" else incorrect_zero)
                  for p in study.POLICIES}
        scores = study.score_comparisons(dict(real_residue=result))
        self.assertTrue(all(s["status"] == "REJECTED_SCREEN_REGRESSION" for s in scores.values()))

    def test_improvement_elsewhere_cannot_hide_a_dose_regression(self):
        base = dict(return_windows={"late": dict(return_error_ug=40_000)},
                    dose={"pause10": dict(added_error_ug=0)})
        candidate = dict(return_windows={"late": dict(return_error_ug=10_000)},
                         dose={"pause10": dict(added_error_ug=-10_000)})
        result = {p: dict(metrics=base if p == "baseline" else candidate) for p in study.POLICIES}
        for score in study.score_comparisons(dict(case=result)).values():
            self.assertEqual(score["status"], "REJECTED_SCREEN_REGRESSION")
            self.assertEqual(len(score["improvements"]), 1)
            self.assertEqual(len(score["regressions"]), 1)

    def test_identical_policy_is_not_promoted_without_improvement(self):
        metrics = dict(return_windows={"late": dict(return_error_ug=500)})
        result = {p: dict(metrics=metrics) for p in study.POLICIES}
        for score in study.score_comparisons(dict(case=result)).values():
            self.assertEqual(score["status"], "NO_MATERIAL_IMPROVEMENT")


if __name__ == "__main__":
    unittest.main()
