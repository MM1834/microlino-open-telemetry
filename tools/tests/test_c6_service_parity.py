from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
C6 = ROOT / "firmware/esp32-c6/src"
CONFIG = (C6 / "c6_config.cpp").read_text(encoding="utf-8")
WEB = (C6 / "c6_web.cpp").read_text(encoding="utf-8")
MAIN = (C6 / "main.cpp").read_text(encoding="utf-8")
ABRP = (ROOT / "firmware/common/abrp/abrp_client.cpp").read_text(encoding="utf-8")
WROOM_ADAPTER = (ROOT / "firmware/esp32-wroom/src/abrp/wroom_abrp.cpp").read_text(encoding="utf-8")


class C6ServiceParityTests(unittest.TestCase):
    def test_common_abrp_has_no_board_specific_dependencies(self) -> None:
        self.assertNotIn("app_config.h", ABRP)
        self.assertNotIn("wroom_gps.h", ABRP)
        self.assertIn("AbrpLocationProvider", (ROOT / "firmware/common/abrp/abrp_client.h").read_text())
        self.assertIn("setupAbrp(settings())", WROOM_ADAPTER)

    def test_abrp_send_is_off_main_loop(self) -> None:
        self.assertIn("xTaskCreate(sendTask", ABRP)
        self.assertIn("currentStatus.inFlight", ABRP)
        self.assertIn("c6AbrpLoop();", MAIN)
        self.assertIn("c6AwsLoop();", MAIN)

    def test_c6_persists_service_state_but_redacts_secrets(self) -> None:
        self.assertIn('preferences.putBool("abrpEn"', CONFIG)
        self.assertIn('preferences.putString("abrpKey"', CONFIG)
        self.assertIn('doc["abrpEnabled"]', CONFIG)
        secret_section = CONFIG.split("String c6ConfigExportJson", 1)[1].split("bool c6ConfigImportJson", 1)[0]
        self.assertIn('if (includeSecrets)', secret_section)
        self.assertNotIn('doc["abrpApiKey"] = c6Config.abrpApiKey;\n    doc', secret_section)

    def test_authenticated_abrp_and_onboarding_routes_exist(self) -> None:
        for route in (
            '"/wizard"', '"/api/onboarding"', '"/api/onboarding/complete"',
            '"/api/onboarding/restart"', '"/api/abrp/status"', '"/api/abrp/test"',
        ):
            self.assertIn(route, WEB)
        self.assertIn("LocalWebSecurity::requireSameOrigin(server)", WEB)
        self.assertIn("if (!requireAdmin()) return", WEB)

    def test_existing_admin_installations_migrate_without_forced_wizard(self) -> None:
        self.assertIn('preferences.isKey("onboarded")', CONFIG)
        self.assertIn("validAdminPassword(c6Config.adminPassword)", CONFIG)
        self.assertIn("c6Config.onboardingComplete", WEB)


if __name__ == "__main__":
    unittest.main()
