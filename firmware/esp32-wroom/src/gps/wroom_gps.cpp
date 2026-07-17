#include "wroom_gps.h"

#include <MotGps.h>

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

void wroomGpsLoop() { gps.loop(); }
bool wroomGpsStarted() { return gps.started(); }
bool wroomGpsSeen() { return gps.seen(); }
bool wroomGpsValid() { return gps.valid(); }
double wroomGpsLatitude() { return gps.latitude(); }
double wroomGpsLongitude() { return gps.longitude(); }
double wroomGpsSpeedKmph() { return gps.speedKmph(); }
double wroomGpsAltitudeMeters() { return gps.altitudeMeters(); }
double wroomGpsCourseDegrees() { return gps.courseDegrees(); }
uint32_t wroomGpsSatellites() { return gps.satellites(); }
double wroomGpsHdop() { return gps.hdop(); }
uint32_t wroomGpsLocationAgeMs() { return gps.locationAgeMs(); }
String wroomGpsStatusJson() { return gps.statusJson(); }
