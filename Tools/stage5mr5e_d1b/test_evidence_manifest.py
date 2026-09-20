import json
import tempfile
import unittest
from pathlib import Path

import evidence_manifest


class EvidenceManifestTests(unittest.TestCase):
    def test_build_and_detect_change(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "data.csv").write_bytes(b"a,b\n1,2\n")
            value = evidence_manifest.build(root, "abc123")
            self.assertEqual(1, len(value["files"]))
            self.assertEqual([], evidence_manifest.verify(root))
            (root / "data.csv").write_bytes(b"changed\n")
            self.assertEqual(["sha256: data.csv"],
                evidence_manifest.verify(root))

    def test_manifest_does_not_hash_itself(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "run_manifest_v2.json").write_text("{}",
                encoding="utf-8")
            value = evidence_manifest.build(root, "abc123")
            self.assertEqual([], value["files"])
            self.assertEqual("abc123", json.loads(
                (root / "run_manifest_v2.json").read_text(
                    encoding="utf-8"))["repository_commit"])


if __name__ == "__main__":
    unittest.main()
