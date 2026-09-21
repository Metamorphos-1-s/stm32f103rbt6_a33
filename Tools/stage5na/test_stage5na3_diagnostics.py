import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class Stage5NA3IsolationTests(unittest.TestCase):
    def test_diagnostic_module_has_no_output_or_public_protocol_dependency(self):
        source = (ROOT / "Diagnostics" / "stage5na3_fault_injection.c").read_text(
            encoding="utf-8")
        for forbidden in ("AlarmOutput", "OutputGpio", "Modbus", "Ble",
                          "LimitChecker", "CheckweighShadow"):
            self.assertNotIn(forbidden, source)

    def test_integration_gates_input_valid_not_candidate_output(self):
        source = (ROOT / "App" / "metrology_manager.c").read_text(
            encoding="utf-8")
        self.assertIn("input.valid = Stage5NA3FaultInjection_ApplyInputValid",
                      source)
        self.assertIn("Stage5NA3FaultInjection_ObserveCandidate", source)
        self.assertNotIn("s_alarm_shadow.dynamic_confirmed =", source)
        self.assertNotIn("s_alarm_shadow.last_static_class =", source)

    def test_diagnostic_build_is_opt_in_and_requires_beta(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        config = (ROOT / "Config" / "project_config.h").read_text(
            encoding="utf-8")
        self.assertIn("option(A33_ENABLE_STAGE5NA3_DIAGNOSTICS", cmake)
        self.assertIn("Stage 5N-A3 diagnostics require Stage 5M-R5 Beta", cmake)
        self.assertIn("#define A33_ENABLE_STAGE5NA3_DIAGNOSTICS 0U", config)

    def test_diagnostic_ram_tradeoff_is_build_local(self):
        source = (ROOT / "Services" / "ble_command_service" /
                  "ble_command_service.c").read_text(encoding="utf-8")
        self.assertIn("#if (A33_ENABLE_STAGE5NA3_DIAGNOSTICS != 0U)", source)
        self.assertIn("#define BLE_COMMAND_CACHE_DEPTH 1U", source)
        self.assertIn("#define BLE_COMMAND_CACHE_DEPTH 4U", source)


if __name__ == "__main__":
    unittest.main()
