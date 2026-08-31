import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LANDING = ROOT / "build" / "landing" / "current"
HOME_HTML = (LANDING / "index.html").read_text(encoding="utf-8")
ONBOARDING_HTML = (LANDING / "onboarding" / "index.html").read_text(encoding="utf-8")
HTML = HOME_HTML + ONBOARDING_HTML
CSS = (LANDING / "css" / "site.css").read_text(encoding="utf-8")
I18N = (LANDING / "js" / "i18n.js").read_text(encoding="utf-8")
FLOW = (LANDING / "js" / "onboarding-flow.js").read_text(encoding="utf-8")


class LandingI18nOnboardingTests(unittest.TestCase):
    def test_language_selector_and_local_scripts_are_present(self):
        self.assertIn('<html lang="de">', HTML)
        self.assertIn('id="language-select"', HTML)
        self.assertIn('<option value="de">DE</option>', HTML)
        self.assertIn('<option value="en">EN</option>', HTML)
        self.assertIn('<option value="fr">FR</option>', HTML)
        self.assertIn('href="onboarding/"', HOME_HTML)
        self.assertNotIn('data-flow-part="vehicle"', HOME_HTML)
        self.assertIn('src="../js/i18n.js?v=', ONBOARDING_HTML)
        self.assertIn('src="../js/onboarding-flow.js?v=', ONBOARDING_HTML)
        self.assertIn('href="../css/site.css?v=', ONBOARDING_HTML)
        self.assertNotIn("https://", I18N + FLOW)

    def test_every_translation_key_exists_in_all_three_languages(self):
        keys = set(re.findall(r'data-i18n(?:-aria-label|-alt)?="([A-Za-z0-9]+)"', HTML))
        self.assertGreater(len(keys), 70)
        for key in keys:
            with self.subTest(key=key):
                self.assertGreaterEqual(I18N.count(f"{key}:"), 3)

    def test_onboarding_flow_keeps_user_centered_and_portal_on_right(self):
        self.assertIn("grid-template-columns: minmax(0,1.05fr)", CSS)
        self.assertIn(".user-node { grid-column: 3", CSS)
        self.assertIn(".access-portal { grid-column: 5", CSS)
        self.assertIn(".arrow-left::after", CSS)
        self.assertIn(".arrow-right::after", CSS)
        self.assertIn('data-access="ap"', ONBOARDING_HTML)
        self.assertIn('data-access="local-ip"', ONBOARDING_HTML)
        self.assertIn('data-access="portal"', ONBOARDING_HTML)
        self.assertIn("outline: 3px solid rgba(103,232,249,.72)", CSS)

    def test_usb_c_power_is_drawn_from_vehicle_to_adapter(self):
        vehicle_to_adapter = ONBOARDING_HTML.split('data-flow-part="vehicle"', 1)[1].split(
            'data-flow-part="adapter"', 1
        )[0]
        self.assertIn("OBD-II / CAN", vehicle_to_adapter)
        self.assertIn("USB-C", vehicle_to_adapter)
        self.assertIn('data-i18n="obdUsb"', vehicle_to_adapter)

    def test_phase_interaction_updates_routes_and_localized_detail(self):
        self.assertIn("activeParts", FLOW)
        self.assertIn("activeAccess", FLOW)
        self.assertIn("aria-pressed", FLOW)
        self.assertIn("mot-language-change", FLOW)
        for language in ('de:', 'en:', 'fr:'):
            self.assertIn(language, FLOW)


if __name__ == "__main__":
    unittest.main()
