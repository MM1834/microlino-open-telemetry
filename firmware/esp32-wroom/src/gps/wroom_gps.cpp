#include "wroom_gps.h"

#include <MotGps.h>
#include "../app_config.h"

#ifndef MOT_GPS_RX_PIN
#define MOT_GPS_RX_PIN 16
#endif
#ifndef MOT_GPS_TX_PIN
#define MOT_GPS_TX_PIN 17
#endif
#ifndef MOT_GPS_BAUD
#define MOT_GPS_BAUD 9600
#endif

namespace {
MotGps gps;
}

void setupWroomGps()
{
    if (!config.gpsEnabled) {
        Serial.println("ESP32-WROOM GPS: disabled by configuration");
        return;
    }
    MotGpsConfig config;
    config.rxPin = MOT_GPS_RX_PIN;
    config.txPin = MOT_GPS_TX_PIN;
    config.baud = MOT_GPS_BAUD;
    config.serialPort = 2;
    config.validFixMaxAgeMs = 5000;
    config.setSystemTimeFromGps = true;

    if (!gps.begin(config)) {
        Serial.printf("ESP32-WROOM GPS: startup failed: %s\n", gps.status().message.c_str());
        return;
    }

    Serial.println("ESP32-WROOM GPS: optional L76K receiver configured");
}

void wroomGpsLoop() { if (config.gpsEnabled) gps.loop(); }
bool wroomGpsStarted() { return gps.started(); }
bool wroomGpsSeen() { return gps.seen(); }
bool wroomGpsDetected() { return gps.status().detected; }
bool wroomGpsValid() { return gps.valid(); }
String wroomGpsState() { return String(gps.stateName()); }
double wroomGpsLatitude() { return gps.latitude(); }
double wroomGpsLongitude() { return gps.longitude(); }
double wroomGpsSpeedKmph() { return gps.speedKmph(); }
double wroomGpsAltitudeMeters() { return gps.altitudeMeters(); }
double wroomGpsCourseDegrees() { return gps.courseDegrees(); }
uint32_t wroomGpsSatellites() { return gps.satellites(); }
double wroomGpsHdop() { return gps.hdop(); }
uint32_t wroomGpsLocationAgeMs() { return gps.locationAgeMs(); }
String wroomGpsUtc() {
    char utc[32];
    return gps.utcIso8601(utc, sizeof(utc)) ? String(utc) : String();
}
String wroomGpsStatusJson() { return gps.statusJson(); }
