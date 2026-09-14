import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN_HTML = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
MAIN_JS = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
HTML = (ROOT / "build/dashboard/current/drive/index.html").read_text(encoding="utf-8")
JS = (ROOT / "build/dashboard/current/js/drive.js").read_text(encoding="utf-8")
CSS = (ROOT / "build/dashboard/current/css/drive.css").read_text(encoding="utf-8")
I18N = (ROOT / "build/dashboard/current/js/i18n.js").read_text(encoding="utf-8")


class DashboardChargeRangeContractTests(unittest.TestCase):
    def test_vehicle_trip_fields_use_one_since_charge_summary(self):
        for element_id in ("trip", "consumption", "drive-time"):
            self.assertIn(f'id="{element_id}"', MAIN_HTML)
        renderer = MAIN_JS.split("function renderDriveSinceCharge()", 1)[1].split(
            "async function loadLastCharge", 1
        )[0]
        for field in (
            "distanceKm", "consumptionKwhPer100Km", "durationMinutes",
        ):
            self.assertIn(field, renderer)
        self.assertIn("result?.driveSinceCharge", MAIN_JS)
        self.assertIn("fmtNum(distance, 0)", renderer)
        self.assertIn("20260914-whole-km1", MAIN_HTML)

    def test_drive_view_displays_all_odometer_distances_as_whole_kilometres(self):
        self.assertGreaterEqual(JS.count("maximumFractionDigits: 0"), 3)
        self.assertNotIn("minimumFractionDigits: 1, maximumFractionDigits: 1 })} km", JS)
        self.assertIn("20260914-whole-km1", HTML)

    def test_main_dashboard_shows_last_charge_with_quality(self):
        self.assertIn('id="charge-energy"', MAIN_HTML)
        self.assertIn('id="charge-energy-detail"', MAIN_HTML)
        renderer = MAIN_JS.split("function renderLastCharge()", 1)[1].split(
            "async function loadLastCharge", 1
        )[0]
        for field in (
            "energyKwh", "dischargedKwh", "netKwh", "coveragePercent",
            "socEstimateKwh", "peakSoc", "sessionCount", "finalized",
        ):
            self.assertIn(field, renderer)
        self.assertIn("coverage < 95", renderer)
        self.assertIn("getCurrentJourney", MAIN_JS)
        self.assertIn("year: '2-digit'", renderer)

    def test_drive_page_exposes_distance_odometer_and_charge_reference(self):
        for element_id in (
            "drive-distance", "drive-odometer", "drive-charge-summary",
            "drive-charge-heading",
            "drive-since-charge-distance", "drive-since-charge-zero",
            "drive-since-charge-reserve", "drive-last-charge-energy",
            "drive-last-charge-detail",
        ):
            self.assertIn(f'id="{element_id}"', HTML)
        self.assertIn(".charge-reference-card", CSS)
        self.assertIn("Erwartete Gesamtstrecke bis 0 %", HTML)
        self.assertIn("Erwartete Gesamtstrecke bis Reserve", HTML)
        self.assertLess(
            HTML.index('id="drive-chart"'),
            HTML.index('id="drive-distance"'),
        )
        self.assertLess(
            HTML.index('id="drive-chart"'),
            HTML.index('id="drive-charge-summary"'),
        )
        self.assertIn("20260914-whole-km1", HTML)

    def test_projection_has_conservative_minimum_evidence(self):
        renderer = JS.split("function renderChargeReference()", 1)[1].split(
            "function appendLivePoint", 1
        )[0]
        self.assertIn("distance >= 5", renderer)
        self.assertIn("socUsed >= 5", renderer)
        self.assertIn("distance / socUsed", renderer)
        self.assertIn("rangeReserveSoc", renderer)
        self.assertIn("coverage < 95", renderer)
        self.assertIn("socEstimateKwh", renderer)
        self.assertIn("value === null || value === undefined || value === ''", JS)
        self.assertIn("year: '2-digit'", renderer)
        self.assertIn("lastCharge?.finalized === false", renderer)
        self.assertIn("drive-charge-heading", renderer)
        self.assertIn("Aktueller Ladeblock", renderer)
        self.assertIn("Ladebilanz wird mit der nächsten Fahrt abgeschlossen", renderer)

    def test_all_portal_languages_cover_new_runtime_wording(self):
        for wording in (
            "Seit letzter Ladung",
            "Noch keine Ladereferenz",
            "Hochrechnung ab 5 km und 5 verbrauchten SOC-Punkten",
            "Geladene Energie",
            "Datenabdeckung",
            "SoC-Schätzung",
            "Ladebilanz",
            "Geladen gesamt",
            "Netto im Akku",
            "Entladung vor Fahrt",
            "Ladevorgänge",
            "Ladebilanz offen",
            "Aktueller Ladeblock",
            "SOC-Punkte geladen",
            "Ladebilanz wird mit der nächsten Fahrt abgeschlossen",
        ):
            self.assertGreaterEqual(I18N.count(wording), 3)


if __name__ == "__main__":
    unittest.main()
