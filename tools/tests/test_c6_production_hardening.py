from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "firmware/esp32-c6/src"
CONFIG = (BASE / "c6_config.cpp").read_text(encoding="utf-8")
NETWORK = (BASE / "c6_network.cpp").read_text(encoding="utf-8")
WEB = (BASE / "c6_web.cpp").read_text(encoding="utf-8")
MAIN = (BASE / "main.cpp").read_text(encoding="utf-8")
BOARD = (BASE / "c6_board.cpp").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "firmware/esp32-c6/platformio.ini").read_text(encoding="utf-8")
SHARED_OTA = (ROOT / "firmware/common/web/local_ota.cpp").read_text(encoding="utf-8")
SHARED_SECURITY = (ROOT / "firmware/common/web/local_web_security.cpp").read_text(encoding="utf-8")


class C6ProductionHardeningTests(unittest.TestCase):
    def test_xiao_unified_build_uses_internal_antenna_and_aws_capability(self) -> None:
        xiao = PLATFORMIO.split("[env:xiao-esp32c6]", 1)[1].split("[env:", 1)[0]
        self.assertIn("MOT_XIAO_BOARD=1", xiao)
        self.assertIn("MOT_AWS_IOT=1", xiao)
        self.assertIn("board_build.filesystem = littlefs", xiao)
        self.assertNotIn("MOT_XIAO_EXTERNAL_ANTENNA", xiao)
        self.assertIn("#ifdef MOT_XIAO_EXTERNAL_ANTENNA", BOARD)
        self.assertIn("digitalWrite(14, HIGH)", BOARD)
        self.assertIn("digitalWrite(14, LOW)", BOARD)
        self.assertIn('WiFi antenna: internal ceramic', BOARD)

    def test_c6_has_exactly_two_unified_board_environments(self) -> None:
        environments = re.findall(r"^\[env:([^]]+)]$", PLATFORMIO, re.MULTILINE)
        canonical = [name for name in environments if not name.endswith("-diagnostic")]
        self.assertEqual(canonical, ["nanoesp32c6-n16", "xiao-esp32c6"])
        self.assertIn("nanoesp32c6-n16-soc-diagnostic", environments)
        self.assertNotIn("nanoesp32c6-n16-aws", PLATFORMIO)
        self.assertNotIn("xiao-esp32c6-aws", PLATFORMIO)

    def test_device_id_uses_device_specific_efuse_bytes(self):
        source = (ROOT / "firmware/common/system/device_id.cpp").read_text()
        self.assertIn("(mac >> 40) & 0xFFFFFF", source)
        self.assertNotIn("mac & 0xFFFFFF", source)

    def test_setup_and_fallback_ap_are_always_protected(self) -> None:
        self.assertIn("c6Config.setupPassword", NETWORK)
        self.assertIn("esp_random()", SHARED_SECURITY)
        self.assertIn("WiFi.softAP(motFallbackApSsid().c_str(), value.c_str()", NETWORK)
        self.assertNotIn("WiFi.softAP(motFallbackApSsid().c_str())", NETWORK)
        self.assertIn('server.authenticate("setup", initial.c_str())', WEB)
        self.assertIn('normalized == "setup status"', MAIN)
        self.assertIn('normalized == "admin recover"', MAIN)
        self.assertIn("c6ConfigRecoverAdminPassword", CONFIG)

    def test_admin_password_is_bounded_and_printable(self) -> None:
        self.assertIn("password.length() < 12", CONFIG)
        self.assertIn("password.length() > 63", CONFIG)
        self.assertIn("c < 32 || c > 126", CONFIG)

    def test_operational_routes_are_authenticated(self) -> None:
        for handler in ("statusPage", "configPage", "saveConfig", "exportConfig", "importConfig", "factoryReset"):
            section = WEB.split(f"void {handler}()", 1)[1].split("\n}", 1)[0]
            self.assertIn("requireAdmin()", section, handler)

    def test_mutations_require_same_origin(self) -> None:
        for handler in ("setupSave", "saveConfig", "importConfig", "factoryReset"):
            section = WEB.split(f"void {handler}()", 1)[1].split("\n}", 1)[0]
            self.assertIn("requireSameOrigin", section, handler)

    def test_backup_omits_secrets_and_restore_preserves_boundary(self) -> None:
        self.assertIn("c6ConfigExportJson(false)", WEB)
        self.assertIn("if (includeSecrets)", CONFIG)
        self.assertIn("if (!c6ConfigAdminConfigured())", CONFIG)

    def test_ota_is_opt_in_authenticated_and_recoverable(self) -> None:
        self.assertIn("!settings->enabled", SHARED_OTA)
        self.assertIn("LocalWebSecurity::authenticate", SHARED_OTA)
        self.assertIn("LocalWebSecurity::requireSameOrigin", SHARED_OTA)
        self.assertIn("Update.abort()", SHARED_OTA)
        self.assertIn("The running firmware remains active", SHARED_OTA)

    def test_runtime_services_remain_cooperative(self) -> None:
        loop = MAIN.split("void loop()", 1)[1]
        for call in ("c6DualCanLoop();", "c6GpsLoop();", "c6NetworkLoop();", "c6WebLoop();", "c6AwsLoop();"):
            self.assertIn(call, loop)
        self.assertNotIn("while (WiFi.status()", NETWORK)
        self.assertIn("WiFi.setAutoReconnect(false)", NETWORK)
        self.assertNotIn("WiFi.reconnect();", NETWORK)


if __name__ == "__main__":
    unittest.main()
