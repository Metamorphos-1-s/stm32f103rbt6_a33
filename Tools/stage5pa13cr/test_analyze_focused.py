"""Host-only evidence-tool tests; not hardware qualification."""
import csv
import tempfile
import unittest
from pathlib import Path

from analyze_focused import analyze, SampleController, POLICIES, STATES, REASONS


def row(seq, mode=0, mass=100, model=None):
    if model is not None:
        corrected, offset, state = model.feed(seq, seq * 100, mass, mode == 1)
        reason = model.rebases[-1]['reason'] if model.rebases else 'INITIAL'
    else:
        corrected, offset, state, reason = mass, 0, 'OFF', 'NONE'
    return dict(utc='2026-09-28T00:00:00+00:00', host_monotonic_ns=seq * 100000000,
        firmware=1309, map=261, signature=41276, gross_ug=mass, raw_adc=seq,
        filtered_adc_counts=seq, display_count=0, status_flags=63,
        sample_sequence=seq, mcu_uptime_ms=seq * 100, fault=0, overrun=0,
        dirty=0, revision=19, saved_revision=19, calibration_valid=1,
        mode=mode, state=STATES.index(state), reason=REASONS.index(reason), limited=0,
        offset_ug=offset, candidate_gross_ug=corrected, candidate_uncompensated_ug=mass,
        candidate_sequence=seq, candidate_mcu_ms=seq * 100,
        gate_count=len(model.gates) if model else 0,
        rebuild_count=len(model.rebases) if model else 0,
        boost_samples=model.boost_samples if model else 0,
        obvious_sequence=0, robust_sequence=0, quiet_sequence=0,
        reference_lock_sequence=0, first_correction_sequence=0,
        application=0, save_count=0, checkweigh_mode=0, sample_pair_matched=1,
        aux_fields_note='fixture_not_hardware')


class EvidenceTests(unittest.TestCase):
    def score(self, records):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'fixture.csv'
            with target.open('w', newline='', encoding='utf-8') as stream:
                writer = csv.DictWriter(stream, fieldnames=records[0])
                writer.writeheader()
                writer.writerows(records)
            return analyze(target)

    def test_duplicates_and_gaps_not_interpolated(self):
        result = self.score([row(1), row(1), row(3)])
        self.assertEqual(result['unique_samples'], 2)
        self.assertEqual(result['duplicate_polls'], 1)
        self.assertEqual(result['sample_gaps'][0]['delta_seq'], 2)

    def test_official_mismatch_and_fault_are_visible(self):
        bad = row(2)
        bad['gross_ug'] = 200
        bad['fault'] = 1
        result = self.score([row(1), bad])
        self.assertEqual(result['authoritative_input_mismatches'], 1)
        self.assertEqual(result['safety_bad_samples'][0]['seq'], 2)

    def test_observed_modes_continuous_model_and_numeric_frozen_offset(self):
        model = SampleController(POLICIES[1])
        records = []
        for seq in range(1, 4001):
            mode = 1 if 3200 <= seq < 3500 else 2
            mass = seq * 200 + (500000000 if seq >= 1300 else 0)
            records.append(row(seq, mode, mass, model))
        result = self.score(records)
        self.assertEqual(result['live_python_parity']['mismatches'], 0)
        self.assertEqual(result['maximum_one_sample_offset_change_ug'], 35)
        self.assertEqual(result['maximum_observed_ten_second_offset_change_ug'], 3500)
        self.assertEqual(result['observed_obvious_edges'][0]['step_loss_ug'], 0)
        self.assertEqual(result['dosing_runs'][0]['samples'], 300)
        self.assertTrue(result['dosing_runs'][0]['nonzero'])
        self.assertEqual(result['dosing_runs'][0]['violations'], 0)
        records[3600]['offset_ug'] += 1
        changed = self.score(records)
        self.assertGreater(changed['live_python_parity']['mismatches'], 0)


if __name__ == '__main__':
    unittest.main()
