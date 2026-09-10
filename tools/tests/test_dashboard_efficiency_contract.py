import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HTML = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
APP = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
PROVIDER = (ROOT / "build/dashboard/current/js/providers/aws-backend-provider.js").read_text(encoding="utf-8")
CSS = (ROOT / "build/dashboard/current/css/dashboard.css").read_text(encoding="utf-8")
I18N = (ROOT / "build/dashboard/current/js/i18n.js").read_text(encoding="utf-8")


class DashboardEfficiencyContractTests(unittest.TestCase):
    def test_vehicle_card_has_compact_community_comparison(self):
        self.assertIn('id="efficiency-comparison"', HTML)
        self.assertIn('id="efficiency-personal"', HTML)
        self.assertIn('id="efficiency-community"', HTML)
        self.assertIn('id="efficiency-flag"', HTML)
        self.assertIn(".efficiency-comparison", CSS)

    def test_provider_uses_authenticated_notification_api(self):
        self.assertIn("async getEfficiencyComparison()", PROVIDER)
        self.assertIn("/efficiency-comparison", PROVIDER)
        method = PROVIDER.split("async getEfficiencyComparison()", 1)[1].split("},", 1)[0]
        self.assertIn("notificationRequest", method)

    def test_result_has_absolute_values_and_no_soc_rendering(self):
        renderer = APP.split("function renderEfficiencyComparison()", 1)[1].split(
            "async function loadEfficiencyComparison", 1
        )[0]
        self.assertIn("averageNetKwhPer100Km", renderer)
        self.assertIn("kwhPer100Km", renderer)
        self.assertNotIn("soc", renderer.lower())

    def test_all_languages_cover_comparison_wording(self):
        self.assertGreaterEqual(
            I18N.count("Deine Fahreffizienz im Vergleich zur Community"), 3
        )
        for wording in (
            "Effizienter als die Community",
            "Ähnlich wie die Community",
            "Weniger effizient als die Community",
        ):
            self.assertGreaterEqual(I18N.count(wording), 3)


if __name__ == "__main__":
    unittest.main()
