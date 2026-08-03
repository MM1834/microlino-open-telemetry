from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUTH_MANAGER = ROOT / "dashboard/js/auth/auth-manager.js"
CONFIG_EXAMPLE = ROOT / "dashboard/config.example.js"
BETA_CONFIG_EXAMPLE = ROOT / "dashboard/config.beta.example.js"
PRODUCTION_CONFIG_EXAMPLE = ROOT / "dashboard/config.production.example.js"
CALLBACK_PAGE = ROOT / "dashboard/callback/index.html"


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

    def test_beta_config_uses_exact_hosted_paths(self) -> None:
        source = BETA_CONFIG_EXAMPLE.read_text(encoding="utf-8")
        self.assertIn(
            'redirectUri: "https://www.microlino-open-telemetry.ch/motbeta/callback/"',
            source,
        )
        self.assertIn(
            'logoutUri: "https://www.microlino-open-telemetry.ch/motbeta/"',
            source,
        )
        self.assertNotIn("localhost", source)

    def test_production_config_matches_effective_dashboard_urls(self) -> None:
        source = PRODUCTION_CONFIG_EXAMPLE.read_text(encoding="utf-8")
        self.assertIn(
            'redirectUri: "https://www.microlino-open-telemetry.ch/dashboard/"',
            source,
        )
        self.assertIn(
            'logoutUri: "https://www.microlino-open-telemetry.ch/dashboard/"',
            source,
        )
        self.assertNotIn("localhost", source)
        self.assertNotIn("/motbeta/", source)

    def test_callback_preserves_portal_subdirectory(self) -> None:
        source = CALLBACK_PAGE.read_text(encoding="utf-8")
        self.assertIn("window.location.pathname.replace", source)
        self.assertIn("/callback(?:\\/index\\.html)?\\/?$/", source)
        self.assertNotIn("window.location.replace('/'", source)


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
        self.assertIn("aws-backend-provider.js?v=20260803-4", source)


class DashboardFreshnessTests(unittest.TestCase):
    def test_live_channel_is_distinct_from_obd2_value_freshness(self) -> None:
        app = (ROOT / "dashboard/js/app.js").read_text()
        html = (ROOT / "dashboard/index.html").read_text()
        provider = (ROOT / "dashboard/js/providers/aws-backend-provider.js").read_text()
        self.assertIn("Live-Kanal", html)
        self.assertIn('id="soc-updated"', html)
        self.assertIn("OBD2_FRESHNESS_KEYS", app)
        self.assertIn("updateObd2Freshness()", app)
        self.assertIn("snapshot.metadata?.[key]", provider)
        self.assertIn("{ receivedAt: message.receivedAt }", provider)
        self.assertIn("Stand: ${relativeTime(receivedAt)} · ${stateLabel}", app)


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

    def test_existing_account_can_expand_additional_vehicle_claim_form(self) -> None:
        app = (ROOT / "dashboard/js/app.js").read_text()
        html = (ROOT / "dashboard/index.html").read_text()
        self.assertIn('id="vehicle-add"', html)
        self.assertIn("state.onboardingExpanded", app)
        self.assertIn("function toggleOnboarding()", app)
        self.assertIn("Boolean(state.dataProvider?.claimVehicle)", app)
        self.assertIn("Weiteres Fahrzeug hinzufügen", app)
        self.assertIn("Bestehende Fahrzeuge bleiben zugewiesen", app)
        self.assertIn("$('vehicle-add')?.addEventListener('click', toggleOnboarding)", app)

    def test_example_has_separate_onboarding_api(self) -> None:
        source = CONFIG_EXAMPLE.read_text(encoding="utf-8")
        self.assertIn("onboardingApiBaseUrl:", source)

    def test_admin_claim_ui_requires_token_group_and_backend(self) -> None:
        auth = (ROOT / "dashboard/js/auth/auth-manager.js").read_text()
        provider = (ROOT / "dashboard/js/providers/aws-backend-provider.js").read_text()
        app = (ROOT / "dashboard/js/app.js").read_text()
        html = (ROOT / "dashboard/index.html").read_text()
        self.assertIn("hasGroup(groupName)", auth)
        self.assertIn("auth.hasGroup?.('mot-beta-admins')", app)
        self.assertIn("async issueClaim(vehicleId)", provider)
        self.assertIn("'/api/onboarding/claims'", provider)
        self.assertIn('id="onboarding-admin"', html)
        self.assertIn('id="admin-claim-output"', html)


if __name__ == "__main__":
    unittest.main()
