import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class ShadowIsolationTests(unittest.TestCase):
    def test_candidate_has_no_formal_output_dependencies(self):
        source = (ROOT / "Experimental/stage5na/checkweigh_shadow.c").read_text(
            encoding="utf-8")
        for forbidden in ("OutputGpio", "AlarmOutputManager", "LimitChecker",
                "BleTelemetry", "DisplayController"):
            self.assertNotIn(forbidden, source)

    def test_app_formal_alarm_path_does_not_reference_shadow(self):
        source = (ROOT / "App/app_main.c").read_text(encoding="utf-8")
        marker = "static void App_UpdateAlarmOutputs(uint32_t now_ms)\n{"
        function = source[source.index(marker):]
        function = function[:function.index("#endif")]
        self.assertNotIn("AlarmShadow", function)
        self.assertIn("AlarmOutputManager_Update", function)


if __name__ == "__main__":
    unittest.main()
