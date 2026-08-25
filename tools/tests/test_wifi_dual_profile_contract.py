from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
KEYS = (ROOT / "firmware/common/config/config_keys.h").read_text(encoding="utf-8")
C6_H = (ROOT / "firmware/esp32-c6/src/c6_config.h").read_text(encoding="utf-8")
C6 = (ROOT / "firmware/esp32-c6/src/c6_config.cpp").read_text(encoding="utf-8")
NETWORK = (ROOT / "firmware/esp32-c6/src/c6_network.cpp").read_text(encoding="utf-8")
WEB = (ROOT / "firmware/esp32-c6/src/c6_web.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "firmware/esp32-c6/src/main.cpp").read_text(encoding="utf-8")


class WifiDualProfileContractTests(unittest.TestCase):
    def test_shared_backup_keys_are_explicit(self) -> None:
        self.assertIn('WIFI2_SSID[] = "wifi2Ssid"', KEYS)
        self.assertIn('WIFI2_PASS[] = "wifi2Pass"', KEYS)

    def test_c6_stores_a_second_profile(self) -> None:
        self.assertIn("wifi2Ssid", C6_H)
        self.assertIn("wifi2Password", C6_H)
        self.assertIn('getString("ssid2", "")', C6)

    def test_non_secret_exports_include_ssids_but_not_passwords(self) -> None:
        self.assertIn('doc["wifi2Ssid"]', C6)
        self.assertIn('if (includeSecrets) doc["wifi2Pass"]', C6)

    def test_import_and_clear_paths_cover_second_profile(self) -> None:
        self.assertIn('doc["wifi2Ssid"]', C6)
        self.assertIn("c6ConfigClearWifi2", C6)
        self.assertIn('preferences.remove("ssid2")', C6)

    def test_policy_prefers_home_then_mobile_without_blocking(self) -> None:
        preferred = NETWORK.split("void tryPreferred()", 1)[1].split("\n}", 1)[0]
        self.assertLess(preferred.index("WifiProfile::HOME"), preferred.index("WifiProfile::MOBILE"))
        self.assertIn('beginProfile(WifiProfile::MOBILE, "home timeout")', NETWORK)
        self.assertNotIn("while (WiFi.status()", NETWORK)
        self.assertNotIn("delay(", NETWORK)

    def test_mobile_connection_polls_and_returns_to_visible_home(self) -> None:
        self.assertIn("HOME_SCAN_INTERVAL_MS", NETWORK)
        self.assertIn("WiFi.scanNetworks(true, true)", NETWORK)
        self.assertIn("WiFi.scanComplete()", NETWORK)
        scan = NETWORK.split("void pollHomeScan", 1)[1].split("\n}\n}", 1)[0]
        self.assertIn('if (homeVisible)', scan)
        self.assertIn('beginProfile(WifiProfile::HOME, reason.c_str())', scan)
        self.assertNotIn('homeRssi >= HOME_RECOVER_RSSI_DBM', scan)
        self.assertNotIn('home visible but weak', scan)

    def test_weak_but_connected_home_is_diagnostic_not_forced_offline(self) -> None:
        self.assertIn("HOME_WEAK_RSSI_DBM", NETWORK)
        self.assertIn("WEAK_LINK_GRACE_MS", NETWORK)
        self.assertIn('"home connected but weak ("', NETWORK)
        self.assertNotIn('beginProfile(WifiProfile::MOBILE, "home link persistently weak")', NETWORK)
        self.assertNotIn('scheduleRetry("home link persistently weak")', NETWORK)
        self.assertIn("bool c6NetworkTransportReady() { return c6NetworkOnline(); }", NETWORK)

    def test_stale_connected_status_without_ip_is_treated_as_offline(self) -> None:
        self.assertIn("WiFi.status() == WL_CONNECTED", NETWORK)
        self.assertIn("WiFi.localIP() != IPAddress(0, 0, 0, 0)", NETWORK)
        self.assertIn("const bool online = stationOnline();", NETWORK)

    def test_fallback_ap_waits_for_both_failures_and_station_stability(self) -> None:
        self.assertIn("STABLE_INTERVAL_MS", NETWORK)
        self.assertIn("now - connectedSinceMs >= STABLE_INTERVAL_MS", NETWORK)
        self.assertIn("scheduleRetry", NETWORK)
        self.assertIn("startFallbackAp();", NETWORK)

    def test_fallback_ap_remains_during_incomplete_onboarding(self) -> None:
        self.assertIn("if (!c6Config.onboardingComplete) startFallbackAp();", NETWORK)
        self.assertIn("apActive && c6Config.onboardingComplete", NETWORK)
        self.assertIn("c6NetworkEnsureOnboardingAp", NETWORK)
        self.assertIn("c6NetworkEnsureOnboardingAp();", WEB)

    def test_web_and_serial_surfaces_cover_both_profiles_without_secret_echo(self) -> None:
        self.assertIn("Preferred home WiFi", WEB)
        self.assertIn("Second / mobile hotspot", WEB)
        self.assertIn('name=\'wifi2Password\'', WEB)
        self.assertIn('normalized.startsWith("wifi2 set ")', MAIN)
        self.assertIn('normalized == "wifi2 clear"', MAIN)
        self.assertNotIn("c6Config.wifi2Password +", WEB + MAIN)

    def test_diagnostics_identify_state_profile_and_reason(self) -> None:
        self.assertIn("c6NetworkStateName()", WEB)
        self.assertIn("c6NetworkProfileName()", WEB)
        self.assertIn("c6NetworkReason()", WEB)
        self.assertIn("c6NetworkHomeConfigured()", WEB)
        self.assertIn("c6NetworkMobileConfigured()", WEB)
        self.assertIn('\\"transportReady\\"', WEB)
        self.assertIn('\\"weakForMs\\"', WEB)
        self.assertIn('\\"transitions\\"', WEB)
        self.assertIn('\\"bssid\\"', WEB)
        self.assertIn('\\"channel\\"', WEB)
        self.assertIn('\\"disconnects\\"', WEB)
        self.assertIn('\\"lastDisconnectName\\"', WEB)
        self.assertIn("ARDUINO_EVENT_WIFI_STA_DISCONNECTED", NETWORK)


if __name__ == "__main__":
    unittest.main()
