import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
JOURNEY = (ROOT / "firmware/esp32-c6/src/c6_journey_energy.cpp").read_text()
AWS = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text()
OFFLINE = (ROOT / "firmware/esp32-c6/src/c6_offline_cache.cpp").read_text()
NOTIFICATION = (ROOT / "cloud/aws/notifications/template.yaml").read_text()


class C6JourneyBackfillContractTests(unittest.TestCase):
    def test_existing_cache_flag_controls_separate_journey_journal(self):
        self.assertIn("c6Config.offlineCacheEnabled", JOURNEY)
        self.assertIn('JOURNEY_PATH[] = "/offline-cache/journeys.bin"', JOURNEY)
        self.assertIn("c6JourneyBackfillPurge();", OFFLINE)

    def test_journal_is_bounded_checksummed_and_acknowledged_before_delete(self):
        self.assertIn("JOURNEY_MAX_RECORDS = 32", JOURNEY)
        self.assertIn("recordChecksum", JOURNEY)
        self.assertIn("status != \"accepted\" && status != \"duplicate\"", JOURNEY)
        self.assertLess(JOURNEY.index('doc["accepted"].as<bool>()'),
                        JOURNEY.rindex("removeFirstRecord()"))

    def test_history_and_journey_replay_use_distinct_versioned_topics(self):
        self.assertIn('client.publish("journey/backfill/v1"', JOURNEY)
        self.assertIn('client.subscribe("journey/backfill/ack/v1"', JOURNEY)
        self.assertIn("c6JourneyBackfillHandleAwsMessage", AWS)
        self.assertIn("JourneyBackfillEnabled:", NOTIFICATION)
        self.assertIn('Default: "false"', NOTIFICATION)
        self.assertNotIn("JourneyBackfillVehicleAllowlist", NOTIFICATION)

    def test_payload_excludes_location_and_network_identity(self):
        publish = JOURNEY.split("void c6JourneyBackfillLoop", 1)[1].split(
            "void c6JourneyBackfillHandleAwsMessage", 1
        )[0]
        for forbidden in ("latitude", "longitude", "wifi", "ipAddress"):
            self.assertNotIn(forbidden, publish)


if __name__ == "__main__":
    unittest.main()
