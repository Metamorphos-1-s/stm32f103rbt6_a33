import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "verify_firmware_manifest.py"
SPEC = importlib.util.spec_from_file_location("verify_firmware_manifest", MODULE_PATH)
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class FirmwareManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        result = self.root / "Results/run"
        result.mkdir(parents=True)
        self.probe = result / "probe.json"
        self.region = result / "config_region.bin"
        self.parsed = result / "parsed.json"
        probe_data = [{"status": "PASS", "data": {
            "firmware_version": 0x050F, "register_map_version": 0x0104,
            "schema_version": 2, "config_dirty": 0, "active_slot": 1,
            "storage_sequence": 25}}]
        parsed_data = {"source": "config_region.bin", "active_slot": "A",
                       "modbus_comparison": {"active_slot_matches": True,
                                             "sequence_matches": True},
                       "slots": [{"slot": "A", "valid": True,
                                  "payload_schema_version": 2, "sequence": 25,
                                  "persistent": {"display": {"brightness": 3}}}]}
        self.probe.write_text(json.dumps(probe_data) + "\n", encoding="utf-8")
        self.region.write_bytes(bytes(4096))
        self.parsed.write_text(json.dumps(parsed_data) + "\n", encoding="utf-8")
        self.manifest = self.root / "manifest.json"
        self.record = {
            "manifest_schema": 1, "firmware": "0x050F",
            "register_map": "0x0104", "schema": 2,
            "production_commit": "b" * 40, "evidence_commit": "5" * 40,
            "release_elf_sha256": "A" * 64,
            "final_probe": {"path": "Results/run/probe.json",
                            "sha256": sha256(self.probe),
                            "length": self.probe.stat().st_size,
                            "revision": 25, "saved_revision": 25, "dirty": 0,
                            "active_slot": "A", "sequence": 25, "brightness": 3},
            "final_config_region": {"path": "Results/run/config_region.bin",
                                    "sha256": sha256(self.region),
                                    "length": self.region.stat().st_size,
                                    "parsed_path": "Results/run/parsed.json",
                                    "parsed_sha256": sha256(self.parsed),
                                    "parsed_length": self.parsed.stat().st_size},
        }
        self._write_manifest()

    def tearDown(self):
        self.temporary.cleanup()

    def _write_manifest(self):
        self.manifest.write_text(json.dumps(self.record), encoding="utf-8")

    def test_valid_manifest(self):
        self.assertEqual([], VERIFY.verify_manifest(self.root, self.manifest))

    def test_hash_mismatch_is_rejected(self):
        self.probe.write_bytes(self.probe.read_bytes() + b" ")
        errors = VERIFY.verify_manifest(self.root, self.manifest)
        self.assertTrue(any("SHA-256 mismatch" in error for error in errors))

    def test_semantic_mismatch_is_rejected(self):
        self.record["final_probe"]["brightness"] = 4
        self._write_manifest()
        errors = VERIFY.verify_manifest(self.root, self.manifest)
        self.assertTrue(any("active slot brightness" in error for error in errors))

    def test_path_escape_is_rejected(self):
        self.record["final_probe"]["path"] = "../probe.json"
        self._write_manifest()
        errors = VERIFY.verify_manifest(self.root, self.manifest)
        self.assertTrue(any("escapes repository root" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
