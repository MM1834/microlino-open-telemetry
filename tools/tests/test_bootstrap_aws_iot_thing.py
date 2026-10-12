from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = (ROOT / "tools/bootstrap_aws_iot_thing.sh").read_text(encoding="utf-8")


class BootstrapAwsIotThingTests(unittest.TestCase):
    def test_board_type_is_explicit_and_validated(self) -> None:
        self.assertIn('BOARD_TYPE="${4:-esp32-wroom}"', SCRIPT)
        self.assertIn('"lilygo-t-a7670"', SCRIPT)
        self.assertIn('"wican-lite"', SCRIPT)
        self.assertIn('if [[ "$BOARD_TYPE" == "wican-lite" ]]', SCRIPT)
        lite_policy = SCRIPT.split(
            'if [[ "$BOARD_TYPE" == "wican-lite" ]]', 1
        )[1].split("else", 1)[0]
        self.assertNotIn('"iot:Subscribe"', lite_policy)
        self.assertNotIn('"iot:Receive"', lite_policy)
        self.assertIn("boardType=${BOARD_TYPE}", SCRIPT)
        self.assertNotIn("boardType=esp32-wroom}", SCRIPT)

    def test_standard_policy_allows_every_firmware_backfill_ack(self) -> None:
        for suffix in (
            "history/backfill/ack/v1",
            "journey/backfill/ack/v1",
            "charge/backfill/ack/v1",
        ):
            self.assertIn(f"topicfilter/mot/${{VEHICLE_ID}}/{suffix}", SCRIPT)
            self.assertIn(f"topic/mot/${{VEHICLE_ID}}/{suffix}", SCRIPT)


if __name__ == "__main__":
    unittest.main()
