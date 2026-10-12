from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AWS = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text(encoding="utf-8")
JOURNEY = (ROOT / "firmware/esp32-c6/src/c6_journey_energy.cpp").read_text(encoding="utf-8")
CLIENT = (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.cpp").read_text(encoding="utf-8")


class C6AwsStateEnvelopeTests(unittest.TestCase):
    def test_periodic_telemetry_is_one_bounded_versioned_publish(self):
        publish = AWS.split("bool publishTelemetry()", 1)[1].split("\n}", 1)[0]
        self.assertIn('document["version"] = 1', publish)
        self.assertIn('document["vehicleId"] = credentials.vehicleId', publish)
        self.assertIn('document["sequence"] = ++telemetrySequence', publish)
        self.assertIn('document["sampledAt"]', publish)
        self.assertIn('document["values"].to<JsonObject>()', publish)
        self.assertIn('client.publish("telemetry/state/v1", payload, true)', publish)
        self.assertIn("TELEMETRY_ENVELOPE_MAX_BYTES", publish)
        self.assertNotIn("client.publishFloat(", publish)
        self.assertNotIn("client.publishInt(", publish)
        self.assertNotIn("client.publishBool(", publish)

    def test_envelope_preserves_canonical_signal_suffixes(self):
        for suffix in (
            "display/soc", "display/speed_kmh", "display/odometer_km",
            "charging/is_charging", "charging/plugged", "charging/power_signed",
            "bms/pack_voltage", "bms/pack_current", "bms/vehicle_power_w",
            "access/central_lock_valid", "location/latitude", "location/altitude_m",
        ):
            self.assertIn(f'values["{suffix}"]', AWS)

    def test_journey_live_counters_join_envelope_not_scalar_topics(self):
        self.assertIn("c6JourneyEnergyAddToEnvelope(values)", AWS)
        self.assertIn("c6JourneyEnergyEnvelopePublished()", AWS)
        self.assertNotIn("c6JourneyEnergyPublish", AWS)
        self.assertIn('values["journey/energy_counter_id"]', JOURNEY)

    def test_mqtt_buffer_has_bounded_envelope_capacity(self):
        self.assertIn("mqtt_.setBufferSize(5120)", CLIENT)
        self.assertIn("TELEMETRY_ENVELOPE_MAX_BYTES = 4096", AWS)


if __name__ == "__main__":
    unittest.main()
