"""Offline generator tests; never open COM5 or modify source recordings."""

import json
import tempfile
import unittest
from pathlib import Path

from build_interactive_waveform import build, read_samples


HEADER = "utc,host_monotonic_ns,gross_ug,display_count,display_decimals,raw_adc,filtered_raw\n"


class WaveformGeneratorTests(unittest.TestCase):
    def test_handles_missing_optional_channel_and_escapes_html(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            input_file = root / "scale & sample.csv"
            input_file.write_text(HEADER +
                "2026-09-25T08:00:00.000Z,1000000000,1000000,100,2,-10,-11\n" +
                "2026-09-25T08:00:00.500Z,1500000000,2000000,200,2,-12,-13\n",
                encoding="utf-8")
            output = root / "plot.html"
            result = build(input_file, output)
            self.assertEqual(result["points"], 2)
            self.assertIn("conditioned_display_ug", result["missing_fields"])
            html = output.read_text(encoding="utf-8")
            self.assertIn("\\u0026", html)
            self.assertNotIn("__WAVEFORM_DATA__", html)
            self.assertIn("[500,2.0,2.0,null,-12.0,-13.0]", html)

    def test_rejects_missing_required_or_non_monotonic(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "samples.csv"
            source.write_text("utc,host_monotonic_ns,display_count\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "lacks UTC"):
                read_samples(source)
            source.write_text(HEADER +
                "2026-09-25T08:00:00Z,100,5,0,2,-1,-1\n" +
                "2026-09-25T08:00:01Z,100,5,0,2,-1,-1\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "strictly increasing"):
                read_samples(source)

    def test_operator_and_measured_edge_remain_separate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            csvfile = root / "samples.csv"
            csvfile.write_text(HEADER +
                "2026-09-25T08:00:00Z,1000000000,0,0,2,-1,-1\n" +
                "2026-09-25T08:00:01Z,2000000000,500000000,50000,2,-2,-2\n",
                encoding="utf-8")
            events = root / "events.jsonl"
            events.write_text(json.dumps({"utc":"2026-09-25T08:00:05Z",
                "event":"LOAD_2_OPERATOR_CONFIRMED"})+"\n", encoding="utf-8")
            timeline = root / "event_timeline.csv"
            timeline.write_text("event,gross_before_utc,gross_after_utc,physical_step_ug\n"
                "load2,2026-09-25T08:00:00Z,2026-09-25T08:00:01Z,500000000\n",
                encoding="utf-8")
            result = build(csvfile, root / "plot.html", events, timeline)
            self.assertEqual(result["event_markers"], 2)
            html = (root / "plot.html").read_text(encoding="utf-8")
            self.assertIn('"kind":"operator"', html)
            self.assertIn('"kind":"edge"', html)


if __name__ == "__main__":
    unittest.main()
