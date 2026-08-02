from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUTH_MANAGER = ROOT / "dashboard/js/auth/auth-manager.js"
CONFIG_EXAMPLE = ROOT / "dashboard/config.example.js"


class DashboardLogoutConfigurationTests(unittest.TestCase):
    def test_logout_uses_dedicated_registered_uri(self) -> None:
        source = AUTH_MANAGER.read_text(encoding="utf-8")
        self.assertIn("!config.logoutUri", source)
        self.assertIn("logout_uri: config.logoutUri", source)
        self.assertNotIn("logout_uri: config.redirectUri", source)

    def test_example_separates_callback_and_logout_destinations(self) -> None:
        source = CONFIG_EXAMPLE.read_text(encoding="utf-8")
        self.assertIn('redirectUri: "https://YOUR_DASHBOARD_URL/callback"', source)
        self.assertIn('logoutUri: "https://YOUR_DASHBOARD_URL/"', source)


if __name__ == "__main__":
    unittest.main()
