from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
C6 = ROOT / "firmware/esp32-c6"
COUNTER = (C6 / "src/c6_journey_energy.cpp").read_text(encoding="utf-8")
AWS = (C6 / "src/c6_aws.cpp").read_text(encoding="utf-8")
MAIN = (C6 / "src/main.cpp").read_text(encoding="utf-8")
PLATFORMIO = (C6 / "platformio.ini").read_text(encoding="utf-8")


class C6JourneyEnergyCounterTests(unittest.TestCase):
    def test_counter_is_n16_only_for_initial_pilot(self) -> None:
        n16 = PLATFORMIO.split("[env:nanoesp32c6-n16]", 1)[1].split(
            "[env:xiao-esp32c6]", 1
        )[0]
        xiao = PLATFORMIO.split("[env:xiao-esp32c6]", 1)[1]
        self.assertIn("MOT_JOURNEY_ENERGY_COUNTER=1", n16)
        self.assertNotIn("MOT_JOURNEY_ENERGY_COUNTER=1", xiao)
        self.assertIn("#ifdef MOT_JOURNEY_ENERGY_COUNTER", COUNTER)

    def test_counter_uses_ram_only_and_bounds_integration_gaps(self) -> None:
        self.assertIn("double drawnWattMs", COUNTER)
        self.assertIn("double regenWattMs", COUNTER)
        self.assertIn("MAX_INTEGRATION_STEP_MS = 2000", COUNTER)
        self.assertIn("stepMs <= MAX_INTEGRATION_STEP_MS", COUNTER)
        self.assertNotIn("Preferences", COUNTER)
        self.assertNotIn("LittleFS", COUNTER)
        self.assertNotIn("File ", COUNTER)

    def test_counter_integrates_consumption_and_regeneration_separately(self) -> None:
        self.assertIn("telemetry.bms.vehiclePowerW", COUNTER)
        self.assertIn("state.drawnWattMs += wattMs", COUNTER)
        self.assertIn("state.regenWattMs -= wattMs", COUNTER)
        self.assertIn("journey/energy_drawn_wh", COUNTER)
        self.assertIn("journey/energy_regen_wh", COUNTER)
        self.assertNotIn("journey/energy_net_wh", COUNTER)

    def test_counter_id_survives_short_stops_and_resets_after_boundary(self) -> None:
        self.assertIn("JOURNEY_STOP_MS = 10UL * 60UL * 1000UL", COUNTER)
        self.assertIn("state.sealed", COUNTER)
        self.assertIn("elapsedAtLeast(nowMs, state.stoppedSinceMs, JOURNEY_STOP_MS)", COUNTER)
        self.assertIn("state.journeySequence++", COUNTER)
        self.assertIn("state.bootNonce = esp_random()", COUNTER)

    def test_checkpoint_is_bounded_and_non_retained(self) -> None:
        self.assertIn("CHECKPOINT_MS = 60000", COUNTER)
        self.assertIn('"journey/energy_counter_id", String(state.counterId), false', COUNTER)
        self.assertIn('"journey/energy_drawn_wh", drawnWh(), false', COUNTER)
        self.assertIn('"journey/energy_regen_wh", regenWh(), false', COUNTER)

    def test_counter_updates_before_aws_publish_and_precedes_charge_boundary(self) -> None:
        loop = MAIN.split("void loop()", 1)[1]
        self.assertLess(loop.index("c6DualCanLoop();"), loop.index("c6JourneyEnergyLoop();"))
        self.assertLess(loop.index("c6JourneyEnergyLoop();"), loop.index("c6AwsLoop();"))
        publish = AWS.split("bool publishTelemetry()", 1)[1].split("return published;", 1)[0]
        self.assertLess(
            publish.index("c6JourneyEnergyPublish(client)"),
            publish.index('publishBool("charging/is_charging"'),
        )


if __name__ == "__main__":
    unittest.main()
