import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class GpsAltitudeContractTests(unittest.TestCase):
    def source(self, relative):
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_all_supported_publishers_expose_altitude_m(self):
        for relative in (
            "firmware/esp32-c6/src/c6_aws.cpp",
            "firmware/esp32-c6/src/c6_local_mqtt.cpp",
            "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp",
        ):
            with self.subTest(relative=relative):
                self.assertIn('"location/altitude_m"', self.source(relative))

    def test_c6_uses_shared_gps_altitude_value(self):
        self.assertIn(
            "gps.altitudeMeters()",
            self.source("firmware/esp32-c6/src/c6_gps.cpp"),
        )


if __name__ == "__main__":
    unittest.main()
