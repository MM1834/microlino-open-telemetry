from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class CanProfileTelemetryTests(unittest.TestCase):
    def test_shared_aws_birth_publishes_both_profile_keys(self) -> None:
        header = (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.h").read_text()
        source = (ROOT / "firmware/shared-libs/MotAwsIot/src/MotAwsIot.cpp").read_text()

        self.assertIn("String can1Profile;", header)
        self.assertIn("String can2Profile;", header)
        firmware = source.index('publish("system/firmware_version"')
        can1 = source.index('publish("system/can1_profile"')
        can2 = source.index('publish("system/can2_profile"')
        self.assertLess(firmware, can1)
        self.assertLess(can1, can2)

    def test_every_aws_firmware_family_supplies_stable_profile_keys(self) -> None:
        expected = {
            "firmware/esp32-c6/src/c6_aws.cpp": (
                "value.can1Profile = decoderProfileKey(c6Config.can1Profile);",
                "value.can2Profile = decoderProfileKey(c6Config.can2Profile);",
            ),
            "firmware/esp32-wroom/src/mqtt/mqtt_client.cpp": (
                "runtime.can1Profile = decoderProfileKey(config.can1Profile);",
                "runtime.can2Profile = decoderProfileKey(config.can2Profile);",
            ),
            "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp": (
                "runtime.can1Profile = decoderProfileKey(config.canProfile);",
                "runtime.can2Profile = decoderProfileKey(config.can2Profile);",
            ),
        }

        for relative_path, expressions in expected.items():
            source = (ROOT / relative_path).read_text()
            with self.subTest(path=relative_path):
                self.assertIn(expressions[0], source)
                self.assertIn(expressions[1], source)

    def test_topic_contract_documents_retained_birth_state(self) -> None:
        contract = (ROOT / "docs/api/mqtt-topics.md").read_text()
        self.assertIn("`system/can1_profile`", contract)
        self.assertIn("`system/can2_profile`", contract)
        self.assertIn("generic State ingestion path stores", contract)


if __name__ == "__main__":
    unittest.main()
