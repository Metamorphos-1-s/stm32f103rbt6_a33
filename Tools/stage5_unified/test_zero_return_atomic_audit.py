"""Adversarial cases for offline atomic zero-return evidence analysis."""
import unittest
import tempfile
from pathlib import Path

from audit_zero_return_atomic_0530 import (audit, compare, decode_authority,
    decode_candidate, find_edges, window)


def words_at(words, start, size, value):
    raw = (value % (1 << (size * 16))).to_bytes(size * 2, "big")
    words[start:start + size] = [int.from_bytes(raw[i:i + 2], "big")
                               for i in range(0, len(raw), 2)]


def authority_words(apply=1):
    words = [0] * 40
    words_at(words, 0, 2, 0xA13E0001)
    words_at(words, 14, 2, -2147483640)
    words_at(words, 16, 2, apply)
    mass, tare = -5_000_000_000, -8000
    gross = mass - (-2147483640 if apply else 0)
    for start, value in ((18, mass), (22, gross), (26, gross - tare), (30, tare)):
        words_at(words, start, 4, value)
    words_at(words, 38, 2, -1)
    return words


def row(second, mass=100, offset=0):
    return dict(host_monotonic_ns=round(second * 1e9), utc=str(second),
        authority=dict(uncompensated_ug=mass, applied_offset_ug=offset,
                       gross_ug=mass - offset, net_ug=mass - offset, tare_ug=0))


class AtomicEvidenceTests(unittest.TestCase):
    def test_wrong_dataset_is_refused_before_analysis(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "manifest.json").write_text('{"files": []}')
            with self.assertRaisesRegex(ValueError, "manifest pin mismatch"):
                audit(root)

    def test_signed_32_and_64_bit_weights(self):
        result = decode_authority(authority_words())
        self.assertEqual(result["uncompensated_ug"], -5_000_000_000)
        self.assertEqual(result["applied_offset_ug"], -2147483640)
        self.assertEqual(result["candidate_offset_ug"], -1)
        self.assertEqual(result["tare_ug"], -8000)

    def test_disabled_application_does_not_subtract_offset(self):
        result = decode_authority(authority_words(apply=0))
        self.assertEqual(result["gross_ug"], result["uncompensated_ug"])

    def test_corrupted_equation_is_refused(self):
        words = authority_words()
        words[29] ^= 1
        with self.assertRaisesRegex(ValueError, "equation"):
            decode_authority(words)

    def test_authority_signature_and_short_response_are_refused(self):
        for words in ([0] * 40, authority_words()[:-1]):
            with self.assertRaisesRegex(ValueError, "signature/count"):
                decode_authority(words)

    def test_candidate_signature_is_checked(self):
        with self.assertRaises(ValueError):
            decode_candidate([0] * 37)

    def test_full_window_covers_endpoints(self):
        result = window([row(i / 10) for i in range(300)], 0, 0, 30)
        self.assertTrue(result["complete"])

    def test_one_sample_is_not_a_complete_30_second_window(self):
        self.assertFalse(window([row(15)], 0, 0, 30)["complete"])

    def test_partial_tail_is_incomplete(self):
        result = window([row(i / 10) for i in range(150)], 0, 0, 30)
        self.assertEqual(result["status"], "INCOMPLETE_WINDOW")

    def test_middle_gap_is_not_hidden_by_endpoint_coverage(self):
        rows = [row(i / 10) for i in range(300) if not 100 <= i <= 110]
        self.assertEqual(window(rows, 0, 0, 30)["status"], "INCOMPLETE_WINDOW")

    def test_next_load_is_not_an_empty_window(self):
        rows = [row(i / 10, mass=500_000_000 if i >= 100 else 0) for i in range(300)]
        self.assertEqual(window(rows, 0, 0, 30)["status"], "MIXED_LOAD_STATES")

    def test_gap_cannot_create_an_observed_edge(self):
        self.assertEqual(find_edges([row(0), row(10, mass=500_000_000)]), [])
        self.assertEqual(find_edges([row(0), row(.1, mass=500_000_000)])[0]["direction"], "LOAD")

    def test_means_decompose_even_when_medians_do_not(self):
        before = window([row(i / 10, mass=0) for i in range(30)], 0, 0, 3)
        after = window([row(i / 10, mass=(0, 100, 100)[i % 3],
                            offset=(0, 0, 100)[i % 3]) for i in range(30)], 0, 0, 3)
        result = compare(before, after)
        self.assertAlmostEqual(result["mean_delta_ug"]["net_ug"], 100 / 3)
        self.assertAlmostEqual(result["equation_residual_ug"], 0)
        self.assertEqual(result["net_median_delta_ug"], 0)
        self.assertNotEqual(after["stats"]["uncompensated_ug"]["median_ug"] -
                            after["stats"]["applied_offset_ug"]["median_ug"],
                            after["stats"]["net_ug"]["median_ug"])

    def test_incomplete_comparison_does_not_return_numeric_deltas(self):
        result = compare(dict(complete=False), dict(complete=True))
        self.assertNotIn("mean_delta_ug", result)


if __name__ == "__main__":
    unittest.main()
