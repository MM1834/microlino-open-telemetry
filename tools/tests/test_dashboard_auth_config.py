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


class DashboardRevocationTests(unittest.TestCase):
    def test_provider_resynchronizes_assignments_and_stops_live_connection(self) -> None:
        source = (ROOT / "dashboard/js/providers/aws-backend-provider.js").read_text(
            encoding="utf-8"
        )
        self.assertIn("async function syncVehicles(callbacks)", source)
        self.assertIn("if (!exists) activeVehicleId = vehicles[0]?.vehicleId || null", source)
        self.assertIn("liveClient?.stop()", source)
        self.assertIn("Keine aktive Fahrzeugzuordnung", source)
        self.assertIn("window.setInterval(() => refresh(callbacks)", source)

    def test_dashboard_cache_busts_revocation_aware_provider(self) -> None:
        source = (ROOT / "dashboard/index.html").read_text(encoding="utf-8")
        self.assertIn("aws-backend-provider.js?v=20260803-2", source)


class DashboardOnboardingTests(unittest.TestCase):
    def test_empty_assignment_exposes_claim_form(self) -> None:
        provider = (ROOT / "dashboard/js/providers/aws-backend-provider.js").read_text()
        app = (ROOT / "dashboard/js/app.js").read_text()
        html = (ROOT / "dashboard/index.html").read_text()
        self.assertIn("callbacks.onOnboardingRequired?.(true)", provider)
        self.assertIn("async claimVehicle(claim)", provider)
        self.assertIn("'/api/onboarding/claim'", provider)
        self.assertIn("onOnboardingRequired: required => renderOnboarding(required)", app)
        self.assertIn('id="onboarding-form"', html)
        self.assertIn('type="password"', html)

    def test_example_has_separate_onboarding_api(self) -> None:
        source = CONFIG_EXAMPLE.read_text(encoding="utf-8")
        self.assertIn("onboardingApiBaseUrl:", source)


if __name__ == "__main__":
    unittest.main()
