import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class Stage5NBContractTests(unittest.TestCase):
    def test_candidate_algorithm_and_parameters_remain_frozen(self):
        source = (ROOT / "Experimental/stage5na/checkweigh_shadow.c").read_bytes()
        import hashlib
        self.assertEqual(hashlib.sha256(source).hexdigest().upper(),
            "F89034B47AEC92372DD6BE0AEC3947ED27F0008727A268359AE3B172CD607C8F")

    def test_public_map_version_and_beta_only_extension(self):
        source = (ROOT / "Protocol/modbus/modbus_register_map.h").read_text(
            encoding="utf-8")
        self.assertIn("MODBUS_REGISTER_MAP_VERSION 0x0104U", source)
        self.assertIn("A33_ENABLE_STAGE5NB_BETA", source)
        self.assertIn("MODBUS_5NB_FIRST 0x02C0U", source)

    def test_menu_uses_confirm_then_long_apply_contract(self):
        source = (ROOT / "UI/menu_controller/menu_controller.c").read_text(
            encoding="utf-8")
        self.assertIn("CheckweighLocalControl_Confirm();", source)
        self.assertIn("CheckweighLocalControl_Apply();", source)
        self.assertIn("CHECKWEIGH_LOCAL_BUSY", source)
        self.assertIn("CheckweighLocalControl_Cancel();", source)

    def test_no_direct_gpio_dependency_in_active_selector(self):
        source = (ROOT / "Services/guarded_checkweigh/guarded_checkweigh.c").read_text(
            encoding="utf-8")
        for forbidden in ("OutputGpio", "BSP_", "AlarmOutputManager"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
