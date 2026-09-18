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

    def test_altitude_requires_its_own_quality_gate(self):
        header = self.source("firmware/shared-libs/MotGps/src/MotGps.h")
        implementation = self.source("firmware/shared-libs/MotGps/src/MotGps.cpp")
        self.assertIn("minAltitudeSatellites = 6", header)
        self.assertIn("maxAltitudeHdop = 5.0", header)
        self.assertIn("status_.altitudeAgeMs <= config_.validFixMaxAgeMs", implementation)
        self.assertIn("status_.satellites >= config_.minAltitudeSatellites", implementation)
        self.assertIn("status_.hdop <= config_.maxAltitudeHdop", implementation)

    def test_publishers_skip_altitude_when_quality_is_invalid(self):
        for relative, guard in (
            ("firmware/esp32-c6/src/c6_aws.cpp", "c6GpsAltitudePublishable(moving)"),
            ("firmware/esp32-c6/src/c6_local_mqtt.cpp", "c6GpsAltitudePublishable(moving)"),
            ("firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp", "l76kAltitudePublishable(moving)"),
        ):
            with self.subTest(relative=relative):
                self.assertIn(guard, self.source(relative))

    def test_publishers_require_fresh_can_motion_for_altitude(self):
        for relative in (
            "firmware/esp32-c6/src/c6_aws.cpp",
            "firmware/esp32-c6/src/c6_local_mqtt.cpp",
            "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp",
        ):
            with self.subTest(relative=relative):
                source = self.source(relative)
                self.assertIn("altitudePublishable()", source)
                self.assertIn("telemetry.display.lastUpdateMs", source)
                self.assertIn(
                    "telemetry.display.speedKmh > ALTITUDE_MIN_MOVING_SPEED_KMH",
                    source,
                )

    def test_altitude_filter_requires_distance_and_plausible_grade(self):
        implementation = self.source("firmware/shared-libs/MotGps/src/MotGps.cpp")
        self.assertIn("ALTITUDE_MIN_HORIZONTAL_DISTANCE_M = 10.0", implementation)
        self.assertIn("ALTITUDE_BASE_TOLERANCE_M = 10.0", implementation)
        self.assertIn("ALTITUDE_MAX_GRADE = 0.5", implementation)
        self.assertIn("horizontalDistanceMeters", implementation)
        self.assertIn("altitudeRejectedSamples_++", implementation)
        self.assertIn("altitudeAnchorLatitude_ = latitude", implementation)
        self.assertIn("altitudeProvisionalValid_", implementation)
        self.assertIn("altitudeProvisionalLatitude_ = latitude", implementation)
        self.assertIn("altitudeProvisionalMeters_ = altitude", implementation)
        self.assertIn("altitudeFilterProvisional", implementation)

    def test_runtime_diagnostics_separate_raw_and_accepted_altitude(self):
        implementation = self.source("firmware/shared-libs/MotGps/src/MotGps.cpp")
        web = self.source("firmware/esp32-c6/src/c6_web.cpp")
        console = self.source("firmware/esp32-c6/src/main.cpp")
        for field in (
            "rawAltitudeValid",
            "rawAltitudeMeters",
            "altitudeAccepted",
            "altitudeDecision",
            "lastAcceptedAltitudeMeters",
        ):
            self.assertIn(field, implementation)
        self.assertIn("c6GpsStatusJson(gpsMoving)", web)
        self.assertIn("GPS altitude:", console)


if __name__ == "__main__":
    unittest.main()
