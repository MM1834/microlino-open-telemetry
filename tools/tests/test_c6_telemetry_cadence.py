from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
CADENCE = (ROOT / "firmware/esp32-c6/src/c6_telemetry_cadence.cpp").read_text()
MAIN = (ROOT / "firmware/esp32-c6/src/main.cpp").read_text()
AWS = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text()
LOCAL = (ROOT / "firmware/esp32-c6/src/c6_local_mqtt.cpp").read_text()
ABRP = (ROOT / "firmware/esp32-c6/src/c6_abrp.cpp").read_text()


class C6TelemetryCadenceTests(unittest.TestCase):
    def test_shared_state_machine_runs_before_both_transports(self):
        loop = MAIN.split("void loop()", 1)[1]
        update = loop.index("c6TelemetryCadenceUpdate();")
        self.assertLess(update, loop.index("c6AwsLoop();"))
        self.assertLess(update, loop.index("c6LocalMqttLoop();"))

    def test_cadence_has_active_charging_and_two_parked_steps(self):
        self.assertIn("CHARGING_INTERVAL_MS = 30000", CADENCE)
        self.assertIn("PARKED_FIVE_MIN_AFTER_MS = 10UL * 60UL * 1000UL", CADENCE)
        self.assertIn("PARKED_FIVE_MIN_INTERVAL_MS = 5UL * 60UL * 1000UL", CADENCE)
        self.assertIn("PARKED_TEN_MIN_INTERVAL_MS = 10UL * 60UL * 1000UL", CADENCE)
        self.assertIn("Mode::Charging", CADENCE)
        self.assertIn("Mode::ParkedFive", CADENCE)
        self.assertIn("Mode::ParkedTen", CADENCE)

    def test_relevant_transitions_publish_immediately_but_noise_does_not(self):
        for transition in (
            "motionChanged", "chargeChanged", "pluggedChanged",
            "doorChanged", "lockChanged", "peakStarted",
        ):
            self.assertIn(transition, CADENCE)
        event_block = CADENCE.split("if (motionChanged ||", 1)[1].split(
            "eventGeneration++;", 1
        )[0]
        self.assertNotIn("packVoltage", event_block)
        self.assertNotIn("soc", event_block.lower())
        self.assertNotIn("vehiclePowerW", event_block)

    def test_each_transport_uses_own_base_interval_and_shared_events(self):
        self.assertIn("c6Config.publishIntervalMs", AWS)
        self.assertIn("c6Config.localMqttPublishIntervalMs", LOCAL)
        self.assertIn("c6TelemetryCadenceEventGeneration()", AWS)
        self.assertIn("c6TelemetryCadenceEventGeneration()", LOCAL)
        self.assertIn("eventPending", AWS)
        self.assertIn("eventPending", LOCAL)

    def test_abrp_uses_shared_cadence_with_a_sixty_second_base(self):
        self.assertIn("ABRP_ACTIVE_INTERVAL_MS = 60000", ABRP)
        self.assertIn("c6TelemetryCadenceIntervalMs(ABRP_ACTIVE_INTERVAL_MS)", ABRP)
        self.assertIn("c6TelemetryCadenceEventGeneration()", ABRP)
        self.assertIn("queueAbrpTelemetry(current, location)", ABRP)


if __name__ == "__main__":
    unittest.main()
