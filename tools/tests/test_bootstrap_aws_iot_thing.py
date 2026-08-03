from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = (ROOT / "tools/bootstrap_aws_iot_thing.sh").read_text(encoding="utf-8")


class BootstrapAwsIotThingTests(unittest.TestCase):
    def test_board_type_is_explicit_and_validated(self) -> None:
        self.assertIn('BOARD_TYPE="${4:-esp32-wroom}"', SCRIPT)
        self.assertIn('"lilygo-t-a7670"', SCRIPT)
        self.assertIn("boardType=${BOARD_TYPE}", SCRIPT)
        self.assertNotIn("boardType=esp32-wroom}", SCRIPT)


if __name__ == "__main__":
    unittest.main()
