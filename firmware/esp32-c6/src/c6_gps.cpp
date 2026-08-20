#include "c6_gps.h"

#include <MotGps.h>
#include "c6_config.h"

#ifndef MOT_GPS_RX_PIN
#define MOT_GPS_RX_PIN -1
#endif
#ifndef MOT_GPS_TX_PIN
#define MOT_GPS_TX_PIN -1
#endif
#ifndef MOT_GPS_BAUD
#define MOT_GPS_BAUD 9600
#endif

namespace {
MotGps gps;
}

void c6GpsSetup()
{
    if (!c6Config.gpsEnabled) {
        Serial.println("ESP32-C6 GPS: disabled by configuration");
        return;
    }
    MotGpsConfig config;
    config.rxPin = MOT_GPS_RX_PIN;
    config.txPin = MOT_GPS_TX_PIN;
    config.baud = MOT_GPS_BAUD;
    config.serialPort = 1;
    config.validFixMaxAgeMs = 5000;
    config.setSystemTimeFromGps = true;

    if (!gps.begin(config)) {
        Serial.printf("ESP32-C6 GPS: startup failed: %s\n", gps.status().message.c_str());
        return;
    }

    Serial.println("ESP32-C6 GPS: optional receiver configured");
}

void c6GpsLoop()
{
    if (!c6Config.gpsEnabled) return;
    gps.loop();
}

String c6GpsState()
{
    return String(gps.stateName());
}

bool c6GpsDetected()
{
    return gps.detected();
}

bool c6GpsValid()
{
    return gps.valid();
}

bool c6GpsSeen()
{
    return gps.seen();
}

uint64_t c6GpsChars()
{
    return gps.status().chars;
}

String c6GpsMessage()
{
    return gps.status().message;
}

double c6GpsLatitude() { return gps.latitude(); }
double c6GpsLongitude() { return gps.longitude(); }
double c6GpsSpeedKmph() { return gps.speedKmph(); }
uint32_t c6GpsSatellites() { return gps.satellites(); }
