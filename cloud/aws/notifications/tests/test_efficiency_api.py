import importlib
import json
import os
import sys
import unittest
from decimal import Decimal
from pathlib import Path
from unittest.mock import MagicMock, patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
os.environ.setdefault("AWS_DEFAULT_REGION", "eu-north-1")
os.environ.setdefault("ACCESS_TABLE_NAME", "access")
os.environ.setdefault("FLEET_AGGREGATE_TABLE_NAME", "fleet")
os.environ.setdefault("USER_EFFICIENCY_TABLE_NAME", "personal")


class EfficiencyApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        modules = {"boto3": MagicMock()}
        with patch.dict(sys.modules, modules):
            cls.api = importlib.import_module("efficiency_api")

    def test_comparison_excludes_own_values_and_has_plus_flag(self):
        result = self.api.comparison(
            {
                "isFinalized": True, "distanceKm": Decimal("1000"),
                "energyNetKwh": Decimal("80"), "journeyCount": 50,
                "activeVehicleCount": 7,
            },
            {
                "isFinalized": True, "distanceKm": Decimal("100"),
                "energyNetKwh": Decimal("6"), "journeyCount": 5,
                "averageNetKwhPer100Km": Decimal("6"),
            },
            "2026-08",
        )
        self.assertTrue(result["available"])
        self.assertEqual("+", result["flag"])
        self.assertAlmostEqual(900, result["community"]["distanceKm"])
        self.assertNotIn("soc", json.dumps(result).lower())
        self.assertNotIn("battery", json.dumps(result).lower())

    def test_small_remaining_cohort_is_suppressed(self):
        result = self.api.comparison(
            {
                "isFinalized": True, "distanceKm": 500,
                "energyNetKwh": 40, "journeyCount": 20,
                "activeVehicleCount": 3,
            },
            {
                "isFinalized": True, "distanceKm": 100,
                "energyNetKwh": 8, "journeyCount": 5,
                "averageNetKwhPer100Km": 8,
            },
            "2026-08",
        )
        self.assertFalse(result["available"])
        self.assertEqual("not_enough_community_data", result["reason"])

    def test_five_percent_neutral_band(self):
        base = {
            "isFinalized": True, "distanceKm": 1000,
            "energyNetKwh": 80, "journeyCount": 50,
            "activeVehicleCount": 7,
        }
        own = {
            "isFinalized": True, "distanceKm": 100,
            "energyNetKwh": Decimal("8.2"), "journeyCount": 5,
            "averageNetKwhPer100Km": Decimal("8.2"),
        }
        self.assertEqual("=", self.api.comparison(base, own, "2026-08")["flag"])

    def test_previous_month_starts_at_calendar_midnight(self):
        self.assertEqual("2026-08", self.api._previous_month(1789036909))


if __name__ == "__main__":
    unittest.main()
