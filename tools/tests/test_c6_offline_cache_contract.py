import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CACHE = (ROOT / "firmware/esp32-c6/src/c6_offline_cache.cpp").read_text()
CONFIG_H = (ROOT / "firmware/esp32-c6/src/c6_config.h").read_text()
CONFIG_CPP = (ROOT / "firmware/esp32-c6/src/c6_config.cpp").read_text()
WEB = (ROOT / "firmware/esp32-c6/src/c6_web.cpp").read_text()
AWS = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text()
MAIN = (ROOT / "firmware/esp32-c6/src/main.cpp").read_text()
PLATFORMIO = (ROOT / "firmware/esp32-c6/platformio.ini").read_text()
TEMPLATE = (ROOT / "cloud/aws/cache-test/template.yaml").read_text()


class C6OfflineCacheContractTests(unittest.TestCase):
    def test_feature_is_default_off_and_persistent(self):
        self.assertIn("bool offlineCacheEnabled = false;", CONFIG_H)
        self.assertIn('preferences.getBool("cacheEn", false)', CONFIG_CPP)
        self.assertIn('preferences.putBool("cacheEn", c6Config.offlineCacheEnabled)', CONFIG_CPP)
        self.assertIn('doc["offlineCacheEnabled"]', CONFIG_CPP)

    def test_board_storage_caps_are_fixed(self):
        self.assertIn("MOT_OFFLINE_CACHE_MAX_BYTES=131072", PLATFORMIO)
        self.assertIn("MOT_OFFLINE_CACHE_MAX_BYTES=262144", PLATFORMIO)
        self.assertIn("used + sizeof(CacheRecord) > MOT_OFFLINE_CACHE_MAX_BYTES", CACHE)

    def test_only_soc_and_speed_are_serialized(self):
        self.assertIn('CachedSignal : uint8_t { Soc = 1, Speed = 2 }', CACHE)
        self.assertIn('? "soc" : "speed"', CACHE)
        for forbidden in ('"latitude"', '"longitude"', '"odometer"', '"power"'):
            self.assertNotIn(forbidden, CACHE)

    def test_time_and_motion_contract_is_explicit(self):
        self.assertIn("now < MIN_VALID_UTC", CACHE)
        self.assertIn("SOC_INTERVAL_SECONDS = 300", CACHE)
        self.assertIn("SPEED_INTERVAL_SECONDS = 60", CACHE)
        self.assertIn("telemetry.display.speedKmh > 1.0f", CACHE)
        self.assertIn("!moving && state.speedWasActive", CACHE)

    def test_ack_precedes_local_deletion_and_factory_reset_purges(self):
        ack_position = CACHE.index('doc["accepted"].as<bool>()')
        remove_position = CACHE.index("removeAcknowledged(state.pendingBatchCount)")
        self.assertLess(ack_position, remove_position)
        self.assertIn("c6OfflineCachePurge();", CONFIG_CPP)
        self.assertIn("LittleFS.remove(CACHE_PATH)", CACHE)
        self.assertIn("state.replayBlocked = true", CACHE)
        self.assertIn("state.waitingForAck || state.replayBlocked", CACHE)

    def test_live_publish_precedes_replay_and_ack_is_subscribed(self):
        self.assertIn("freshLivePublished = publishTelemetry() || freshLivePublished", AWS)
        self.assertIn("c6OfflineCacheLoop(client, client.connected(), freshLivePublished)", AWS)
        self.assertIn('client.subscribe("history/backfill/ack/v1", 1)', CACHE)
        self.assertIn('client.publish("history/backfill/v1", payload, false)', CACHE)

    def test_authenticated_web_ui_exposes_bounded_control_and_diagnostics(self):
        self.assertIn("name='offlineCacheEnabled'", WEB)
        self.assertIn("Default: disabled", WEB)
        self.assertIn("No GPS/location is stored", WEB)
        self.assertIn("c6OfflineCacheStatusJson()", WEB)
        self.assertIn('normalized == "cache enable"', MAIN)
        self.assertIn('normalized == "cache purge"', MAIN)

    def test_test_cloud_root_cannot_match_operational_rule(self):
        self.assertIn("Default: mot-test", TEMPLATE)
        self.assertIn("FROM '${TopicRoot}/${TestVehicleId}/history/backfill/v1'", TEMPLATE)
        self.assertNotIn("FROM 'mot/#'", TEMPLATE)
        self.assertIn("RetentionInDays: !Ref LogRetentionDays", TEMPLATE)
        self.assertIn("TimeToLiveSpecification", TEMPLATE)
        self.assertIn('Default: "true"', TEMPLATE)
        self.assertIn("AckReceiveEnabled", TEMPLATE)


if __name__ == "__main__":
    unittest.main()
