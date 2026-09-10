import os
import sys
import unittest
from decimal import Decimal
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
os.environ.setdefault("AWS_DEFAULT_REGION", "eu-north-1")

from fleet_efficiency import aggregate_values, previous_zurich_month  # noqa: E402


class FleetEfficiencyTests(unittest.TestCase):
    def event(self, **changes):
        event = {
            "vehicleId": "vehicle-private",
            "journeyId": "journey-private",
            "receivedAt": 1788974575199,
            "distanceKm": Decimal("100"),
            "energyDrawnKwh": Decimal("9"),
            "energyRegenKwh": Decimal("1.5"),
            "energyNetKwh": Decimal("7.5"),
            "socUsed": Decimal("50"),
            "energySource": "firmware_counter",
        }
        event.update(changes)
        return event

    def test_declared_capacity_contributes_to_soc_equivalent(self):
        values = aggregate_values(self.event(), {
            "batteryCapacityKwh": Decimal("15"),
            "batteryCapacityVerificationStatus": "DECLARED",
        })
        self.assertEqual("2026-09", values["month"])
        self.assertEqual(Decimal("7.5"), values["socEquivalentKwh"])
        self.assertEqual(Decimal(1), values["socComparisonIncludedCount"])
        self.assertEqual(Decimal(1), values["firmwareJourneyCount"])

    def test_missing_or_conflicting_capacity_excludes_only_soc_comparison(self):
        for profile in ({}, {
            "batteryCapacityKwh": Decimal("10.5"),
            "batteryCapacityVerificationStatus": "CONFLICT",
        }):
            values = aggregate_values(self.event(), profile)
            self.assertEqual(Decimal("100"), values["distanceKm"])
            self.assertEqual(Decimal("7.5"), values["energyNetKwh"])
            self.assertEqual(Decimal(0), values["socEquivalentKwh"])
            self.assertEqual(Decimal(1), values["socComparisonExcludedCount"])

    def test_marker_is_stable_but_identifiers_are_not_in_aggregate_fields(self):
        first = aggregate_values(self.event(), {})
        second = aggregate_values(self.event(), {})
        changed = aggregate_values(self.event(journeyId="other"), {})
        self.assertEqual(first["markerId"], second["markerId"])
        self.assertNotEqual(first["markerId"], changed["markerId"])
        self.assertNotIn("vehicleId", first)
        self.assertNotIn("journeyId", first)

    def test_telemetry_source_count_is_separate(self):
        values = aggregate_values(self.event(energySource="telemetry_estimate"), {})
        self.assertEqual(Decimal(0), values["firmwareJourneyCount"])
        self.assertEqual(Decimal(1), values["telemetryJourneyCount"])

    def test_previous_month_uses_zurich_calendar(self):
        self.assertEqual("2026-08", previous_zurich_month(1789036909))


if __name__ == "__main__":
    unittest.main()
