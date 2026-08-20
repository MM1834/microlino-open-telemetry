import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class GpsEnableContractTests(unittest.TestCase):
    def source(self, path):
        return (ROOT / path).read_text(encoding="utf-8")

    def test_all_targets_default_to_enabled_and_persist_setting(self):
        cases = [
            ("firmware/esp32-c6/src/c6_config.h", "bool gpsEnabled = true;"),
            ("firmware/esp32-wroom/src/app_config.h", "bool gpsEnabled = true;"),
            ("firmware/lilygo-t-a7670/src/config/lilygo_config.h", "bool gpsEnabled = true;"),
        ]
        for path, token in cases:
            self.assertIn(token, self.source(path))
        self.assertIn('preferences.getBool("gpsEn", true)', self.source("firmware/esp32-c6/src/c6_config.cpp"))
        self.assertIn('prefs.getBool("gpsEn", true)', self.source("firmware/esp32-wroom/src/app_config.cpp"))
        self.assertIn('prefs.getBool("gpsEn", true)', self.source("firmware/lilygo-t-a7670/src/config/lilygo_config.cpp"))

    def test_all_targets_gate_gps_runtime(self):
        for path in (
            "firmware/esp32-c6/src/c6_gps.cpp",
            "firmware/esp32-wroom/src/gps/wroom_gps.cpp",
            "firmware/lilygo-t-a7670/src/gps/l76k_gps.cpp",
        ):
            self.assertIn("gpsEnabled", self.source(path))
            self.assertIn("disabled by configuration", self.source(path))

    def test_configuration_and_wizard_only_offer_detected_or_disabled_gps(self):
        cases = [
            ("firmware/esp32-c6/src/c6_web.cpp", "c6GpsDetected() || !c6Config.gpsEnabled", "/api/gps/toggle"),
            ("firmware/esp32-wroom/src/web/web_ui.cpp", "wroomGpsDetected() || !config.gpsEnabled", "gpsOnly"),
            ("firmware/lilygo-t-a7670/src/web/lilygo_web.cpp", "l76kGpsDetected() || !config.gpsEnabled", "/api/gps/toggle"),
        ]
        for path, condition, submit_contract in cases:
            source = self.source(path)
            self.assertGreaterEqual(source.count(condition), 2)
            self.assertIn(submit_contract, source)
            self.assertIn('name=\'gpsEnabled\'', source)


if __name__ == "__main__":
    unittest.main()
