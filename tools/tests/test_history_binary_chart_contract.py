from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
HISTORY = (ROOT / "build/dashboard/current/js/history/history-chart.js").read_text()
HTML = (ROOT / "build/dashboard/current/index.html").read_text()
CSS = (ROOT / "build/dashboard/current/css/history.css").read_text()
APP = (ROOT / "build/dashboard/current/js/app.js").read_text()


class HistoryBinaryChartContractTests(unittest.TestCase):
    def test_charging_and_plugged_share_one_chart(self) -> None:
        self.assertIn("Ladezustand &amp; Ladekabel", HTML)
        self.assertEqual(1, HTML.count('id="charging-history-chart"'))
        self.assertNotIn('id="plugged-history-chart"', HTML)
        self.assertIn('binarySeries(samples,"charging"', HISTORY)
        self.assertIn('binarySeries(samples,"plugged"', HISTORY)

    def test_plugged_line_is_dashed_and_charging_is_solid(self) -> None:
        self.assertIn('"#a855f7",[])', HISTORY)
        self.assertIn('"#ec4899",[8,5])', HISTORY)
        self.assertIn("ctx.setLineDash(item.dash)", HISTORY)
        self.assertIn("history-legend-line.is-plugged", CSS)

    def test_binary_transitions_are_horizontal_then_vertical(self) -> None:
        self.assertIn("function binaryStepPath(points,endTs)", HISTORY)
        self.assertIn("path.push({ts:current.ts,value:previous.value})", HISTORY)
        self.assertIn("path.push({ts:current.ts,value:current.value})", HISTORY)
        self.assertIn('[[1,"Ein"],[0,"Aus"]]', HISTORY)

    def test_automatic_render_keeps_selected_range_and_button_in_sync(self) -> None:
        self.assertIn("async function render(hours=currentRangeHours)", HISTORY)
        self.assertIn("ALLOWED_RANGES.includes(requestedHours)?requestedHours:currentRangeHours", HISTORY)
        self.assertIn("updateRangeButtons(hours)", HISTORY)
        self.assertNotIn("async function render(hours=24)", HISTORY)

    def test_failed_refresh_keeps_last_successful_charts_visible(self) -> None:
        self.assertIn('updateRequestStatus("History-Aktualisierung fehlgeschlagen · letzte Daten bleiben sichtbar",true)', HISTORY)
        self.assertIn("return false;", HISTORY)
        self.assertIn('id="history-request-status"', HTML)
        self.assertIn("requestId!==latestRenderRequest", HISTORY)
        self.assertIn("inFlightHistoryKey!==requestKey", HISTORY)

    def test_vehicle_poll_does_not_reload_history_every_five_seconds(self) -> None:
        vehicle_callback = APP.split("onVehicles: vehicles => {", 1)[1].split("onOnboardingRequired:", 1)[0]
        self.assertEqual(1, vehicle_callback.count("MOTHistoryChart?.render?.()"))
        self.assertIn("if (selected !== previous)", vehicle_callback)


if __name__ == "__main__":
    unittest.main()
