from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUTH_MANAGER = ROOT / "build/dashboard/current/js/auth/auth-manager.js"
CONFIG_EXAMPLE = ROOT / "build/dashboard/current/config.example.js"
BETA_CONFIG_EXAMPLE = ROOT / "build/dashboard/current/config.beta.example.js"
PRODUCTION_CONFIG_EXAMPLE = ROOT / "build/dashboard/current/config.production.example.js"
CALLBACK_PAGE = ROOT / "build/dashboard/current/callback/index.html"


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
        source = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text(
            encoding="utf-8"
        )
        self.assertIn("async function syncVehicles(callbacks)", source)
        self.assertIn("if (!exists) activeVehicleId = vehicles[0]?.vehicleId || null", source)
        self.assertIn("liveClient?.stop()", source)
        self.assertIn("Keine aktive Fahrzeugzuordnung", source)
        self.assertIn("window.setInterval(() => refresh(callbacks)", source)

    def test_dashboard_cache_busts_revocation_aware_provider(self) -> None:
        source = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
        self.assertIn("aws-backend-provider.js?v=20260901-webflash2", source)
        self.assertIn("app.js?v=20260901-webflash10", source)


class DashboardNotificationSettingsTests(unittest.TestCase):
    def test_portal_exposes_vehicle_scoped_email_settings(self) -> None:
        html = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
        self.assertIn('id="notification-form"', html)
        self.assertIn('id="notification-threshold"', html)
        self.assertIn('id="notification-email"', html)
        self.assertIn('id="notification-journey-email-enabled"', html)
        self.assertIn('id="notification-charging-stop-email-enabled"', html)
        self.assertIn('id="notification-charging-summary-email-enabled"', html)
        self.assertIn('id="notification-charging-stop-threshold"', html)
        self.assertIn('id="range-km-at-100"', html)
        self.assertIn('id="range-reserve-soc"', html)
        self.assertIn("nach mindestens 45 Sekunden Ladezeit", html)
        self.assertIn("mindestens 60 Sekunden stoppt", html)
        self.assertIn("Zusammenfassung geeigneter Fahrten per E-Mail", html)
        self.assertIn('id="notification-sms-enabled" type="checkbox" disabled', html)
        self.assertIn("Persönlich · fahrzeugbezogen", html)
        self.assertIn("AWS Notifications", html)
        self.assertIn("Confirm subscription", html)
        self.assertIn("prüfen Sie deshalb auch den Spam-Ordner", html)
        self.assertIn('id="notification-email-confirmation-help"', html)

    def test_provider_uses_separate_notification_api(self) -> None:
        provider = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text(
            encoding="utf-8"
        )
        self.assertIn("notificationApiBaseUrl", provider)
        self.assertIn("async getNotificationPreferences()", provider)
        self.assertIn("async saveNotificationPreferences(preferences)", provider)

        for config in (CONFIG_EXAMPLE, BETA_CONFIG_EXAMPLE, PRODUCTION_CONFIG_EXAMPLE):
            self.assertIn("notificationApiBaseUrl:", config.read_text(encoding="utf-8"))

    def test_read_requests_retry_transient_capacity_failures(self) -> None:
        provider = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text(
            encoding="utf-8"
        )
        self.assertIn("const RETRYABLE_HTTP_STATUS = new Set([429, 500, 502, 503, 504])", provider)
        self.assertIn("async function fetchWithRetry", provider)
        self.assertIn("250 * (2 ** attempt)", provider)
        self.assertIn("method === 'GET'", provider)

    def test_email_and_sms_status_load_independently(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
        self.assertIn("await Promise.allSettled", app)
        self.assertIn("SMS-Status vorübergehend nicht verfügbar.", app)
        self.assertIn("Ein Teil der Einstellungen ist vorübergehend nicht verfügbar.", app)
        self.assertNotIn("const [preferences, smsStatus] = await Promise.all([", app)

    def test_app_loads_and_saves_for_selected_vehicle(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
        self.assertIn("async function loadNotificationPreferences", app)
        self.assertIn("async function saveNotificationPreferences", app)
        self.assertIn("state.selectedVehicleId", app)
        self.assertIn("getNotificationPreferences", app)
        self.assertIn("saveNotificationPreferences", app)
        self.assertIn("journeyEmailEnabled: $('notification-journey-email-enabled').checked", app)
        self.assertIn("chargingStopEmailEnabled: $('notification-charging-stop-email-enabled').checked", app)
        self.assertIn("chargingSummaryEmailEnabled: $('notification-charging-summary-email-enabled').checked", app)
        self.assertIn("chargingStopThreshold: Number($('notification-charging-stop-threshold').value)", app)
        self.assertIn("rangeKmAt100: Number($('range-km-at-100').value)", app)
        self.assertIn("rangeReserveSoc: Number($('range-reserve-soc').value)", app)
        self.assertIn("responsePreferences?.rangeReserveSoc ?? requestedPreferences.rangeReserveSoc", app)
        self.assertIn("Für Ladestopp-Meldungen zuerst den E-Mail-Kanal aktivieren.", app)
        self.assertIn("Für Fahrtzusammenfassungen zuerst den E-Mail-Kanal aktivieren.", app)
        html = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
        self.assertIn("Pro durchgehend eingesteckter Ladesession", html)
        self.assertIn("Ausstecken und erneutes Einstecken", html)
        self.assertIn("Solar-Nulleinspeisung", html)
        self.assertIn("email.dataset.confirmedEmail", app)
        self.assertIn("help.hidden = stillConfirmed", app)
        self.assertIn("addEventListener('input', updateEmailConfirmationHelp)", app)

    def test_sms_save_captures_opt_in_before_busy_render_and_hides_code(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
        html = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
        capture = app.index("const requestedPreferences = {")
        busy = app.index("state.notificationBusy = true;", capture)
        self.assertLess(capture, busy)
        self.assertIn("saveNotificationPreferences(requestedPreferences)", app)
        self.assertIn("state.smsNotificationStatus.smsEnabled = preferences.smsEnabled === true", app)
        self.assertIn("notification-sms-confirmation-fields", html)
        self.assertIn("status?.verificationStatus !== 'PENDING'", app)


class DashboardFreshnessTests(unittest.TestCase):
    def test_live_channel_is_distinct_from_obd2_value_freshness(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        provider = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text()
        self.assertIn("Live-Kanal", html)
        self.assertIn('id="soc-updated"', html)
        self.assertIn("OBD2_FRESHNESS_KEYS", app)
        self.assertIn("updateObd2Freshness()", app)
        self.assertIn("snapshot.metadata?.[key]", provider)
        self.assertIn("{ receivedAt: message.receivedAt }", provider)
        self.assertIn("Stand: ${relativeTime(receivedAt)} · ${stateLabel}", app)

    def test_charging_and_power_card_marks_stale_topic_values(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        css = (ROOT / "build/dashboard/current/css/dashboard.css").read_text()
        self.assertIn('id="power-updated"', html)
        self.assertIn("CHARGING_FRESHNESS_KEYS", app)
        self.assertIn("POWER_FRESHNESS_KEYS", app)
        self.assertIn("function updatePowerFreshness()", app)
        self.assertIn("Date.now() - receivedAt > VEHICLE_ONLINE_MS", app)
        self.assertIn("Nicht aktuell · letzter Messpunkt ${formatFreshnessTime(receivedAt)}", app)
        self.assertIn("classList.toggle('is-data-stale', stale)", app)
        self.assertIn(".overview-charging.is-data-stale", css)


class DashboardOnboardingTests(unittest.TestCase):
    def test_empty_assignment_exposes_claim_form(self) -> None:
        provider = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text()
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        self.assertIn("callbacks.onOnboardingRequired?.(true)", provider)
        self.assertIn("async claimVehicle(claim)", provider)
        self.assertIn("'/api/onboarding/claim'", provider)
        self.assertIn("onOnboardingRequired: required => renderOnboarding(required)", app)
        self.assertIn('id="onboarding-form"', html)
        self.assertIn('type="password"', html)

    def test_existing_account_can_expand_additional_vehicle_claim_form(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
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
        auth = (ROOT / "build/dashboard/current/js/auth/auth-manager.js").read_text()
        provider = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text()
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        self.assertIn("hasGroup(groupName)", auth)
        self.assertIn("auth.hasGroup?.('mot-beta-admins')", app)
        self.assertIn("async issueClaim(vehicleId)", provider)
        self.assertIn("'/api/onboarding/claims'", provider)
        self.assertIn('id="onboarding-admin"', html)
        self.assertIn('id="admin-claim-output"', html)

    def test_admin_claim_ui_follows_notifications_without_legacy_status_card(self) -> None:
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        css = (ROOT / "build/dashboard/current/css/dashboard.css").read_text()
        self.assertLess(html.index('id="settings"'), html.index('id="onboarding-admin"'))
        self.assertIn('#onboarding-admin{order:75}', css)
        self.assertNotIn('class="card panel status-panel"', html)
        self.assertNotIn('.main>.status-panel{order:80;width:100%}', css)


class DashboardRangeForecastTests(unittest.TestCase):
    def test_shared_portal_renders_personal_forecast_and_soc_comparison(self) -> None:
        app = (ROOT / "build/dashboard/current/js/app.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        history = (ROOT / "build/dashboard/current/js/history/history-chart.js").read_text()
        self.assertIn('id="range-method"', html)
        self.assertIn('id="range-forecast-main"', html)
        self.assertIn('id="range-forecast-basis"', html)
        self.assertIn('id="range-soc-comparison"', html)
        self.assertIn("function renderRangeForecast()", app)
        self.assertIn("Persönliche Prognose", app)
        self.assertIn("Basierend auf ${fmtNum(forecast.distanceKm, 0)} km", app)
        self.assertIn('new CustomEvent("mot-range-forecast"', history)


class DashboardHistoryPowerSignTests(unittest.TestCase):
    def test_history_uses_vehicle_facing_power_sign_without_rewriting_data(self) -> None:
        history = (ROOT / "build/dashboard/current/js/history/history-chart.js").read_text()
        html = (ROOT / "build/dashboard/current/index.html").read_text()
        self.assertIn("power:historyDisplayPower(signedPower)", history)
        self.assertIn("const value=-Number(signedPower)", history)
        self.assertIn("includeZero:true", history)
        self.assertIn("zeroBaseline:true", history)
        self.assertIn("symmetricAroundZero:true", history)
        self.assertIn('formatSignedPower(value)', history)
        self.assertIn("− Verbrauch · + Laden/Rekuperation", html)
        self.assertNotIn("Positiver Anzeigewert", html)


if __name__ == "__main__":
    unittest.main()
