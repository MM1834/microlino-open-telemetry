import types
import unittest

from email_templates import (
    charging_stop, charging_summary, daily_summary, journey_summary, language,
    sms_charging_stop, sms_soc_target, soc_target,
)


class EmailTemplateTests(unittest.TestCase):
    def test_missing_and_unknown_language_fall_back_to_german(self):
        self.assertEqual("de", language({}))
        self.assertEqual("de", language({"notificationLanguage": "xx"}))
        subject, body = soc_target({}, "pioneer", "Pioneer", 80, 81)
        self.assertIn("erreicht", subject)
        self.assertIn("keine Ladesteuerung", body)

    def test_all_languages_cover_all_email_types(self):
        state = types.SimpleNamespace(start_soc=20, last_soc=40, energy_kwh=2.34)
        journey = types.SimpleNamespace(
            distance_km=12.3, duration_minutes=20, soc_used=8,
            energy_drawn_kwh=1.4, energy_regen_kwh=.2,
            energy_net_kwh=1.2, net_kwh_per_100_km=9.8,
            completion_trigger="speed_zero", source_flag="Firmware counter",
        )
        daily = {
            "journeyCount": 2, "distanceKm": 22.2, "journeyDurationMinutes": 40,
            "energyDrawnKwh": 2.5, "energyRegenKwh": .4, "energyNetKwh": 2.1,
            "netKwhPer100Km": 9.5, "chargingCount": 1,
            "chargingDurationMinutes": 60, "energyChargedKwh": 2.3,
            "chargingSocDelta": 20,
        }
        for lang in ("de", "en", "fr", "it"):
            preference = {"notificationLanguage": lang, "vehicleId": "pioneer"}
            messages = [
                soc_target(preference, "pioneer", "Pioneer", 80, 81),
                charging_stop(preference, "pioneer", "Pioneer", 70, 80),
                charging_summary(preference, "pioneer", "Pioneer", state, 60, "unplugged",
                                 capacity_kwh=10.5, coverage_percent=88,
                                 soc_estimate_kwh=2.1),
                journey_summary(preference, "pioneer", "Pioneer", journey),
                daily_summary(preference, "2026-09-10", daily, False),
            ]
            for subject, body in messages:
                self.assertTrue(subject.startswith("MOT -"))
                self.assertTrue(body.startswith("MOT" ) or body.startswith("Résumé") or body.startswith("Riepilogo"))
                self.assertLessEqual(len(subject), 100)
            charging_body = messages[2][1]
            self.assertIn("88", charging_body)
            self.assertIn("2", charging_body)
            sms_messages = (
                sms_soc_target(preference, "pioneer", 81, 80),
                sms_charging_stop(preference, "pioneer", 70, 80),
            )
            for message in sms_messages:
                self.assertLessEqual(len(message), 160)
                self.assertTrue(message.isascii())

    def test_journey_and_daily_distance_use_whole_kilometres(self):
        journey = types.SimpleNamespace(
            distance_km=12.3, duration_minutes=20, soc_used=8,
            energy_drawn_kwh=1.4, energy_regen_kwh=.2,
            energy_net_kwh=1.2, net_kwh_per_100_km=9.8,
            completion_trigger="speed_zero", source_flag="Firmware counter",
        )
        daily = {
            "journeyCount": 1, "distanceKm": 12.3,
            "journeyDurationMinutes": 20, "energyDrawnKwh": 1.4,
            "energyRegenKwh": .2, "energyNetKwh": 1.2,
            "netKwhPer100Km": 9.8, "chargingCount": 0,
            "chargingDurationMinutes": 0, "energyChargedKwh": 0,
            "chargingSocDelta": 0,
        }
        for lang in ("de", "en", "fr", "it"):
            preference = {"notificationLanguage": lang, "vehicleId": "pioneer"}
            subject, body = journey_summary(
                preference, "pioneer", "Pioneer", journey
            )
            self.assertIn("12 km", subject)
            self.assertIn("12 km", body)
            self.assertNotIn("12.3 km", subject + body)
            self.assertNotIn("12,3 km", subject + body)
            _, daily_body = daily_summary(
                preference, "2026-09-14", daily, False
            )
            self.assertIn("12 km", daily_body)


if __name__ == "__main__":
    unittest.main()
