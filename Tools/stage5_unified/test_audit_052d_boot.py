"""Regression for archived programming evidence and readback boundaries."""

import tempfile
import unittest
from pathlib import Path

from audit_052d_boot import audit


ROOT = Path(__file__).resolve().parents[2] / "Results" / "stage5p_052d_display_candidate"


class BootAuditTest(unittest.TestCase):
    def test_historical_evidence_does_not_prove_boot(self):
        result = audit(ROOT)
        self.assertTrue(result["program_verify_succeeded"])
        self.assertTrue(result["postflash_vector_prefix_matches_candidate"])
        self.assertTrue(result["config_unchanged_after_candidate_write"])
        self.assertTrue(result["previous_052c_program_log_confirms_mcu_reset"])
        self.assertFalse(result["explicit_erase_succeeded"])
        self.assertFalse(result["program_log_confirms_mcu_reset"])
        self.assertFalse(result["full_postflash_application_readback_available"])
        self.assertFalse(result["boot_proven"])

    def test_wrong_full_readback_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            wrong = Path(directory) / "wrong.bin"
            # A matching first 256 bytes is insufficient: the rest may be wrong.
            candidate = (ROOT / "software" / "firmware_0x052D.bin").read_bytes()
            wrong.write_bytes(candidate[:256] + b"\xFF" * (126976 - 256))
            result = audit(ROOT, wrong)
            self.assertTrue(result["full_postflash_application_readback_available"])
            self.assertFalse(result["full_postflash_application_matches_candidate_and_erased_tail"])


if __name__ == "__main__":
    unittest.main()
