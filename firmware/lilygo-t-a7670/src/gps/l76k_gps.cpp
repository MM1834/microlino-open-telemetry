#include "l76k_gps.h"

#include <Arduino.h>
#include <MotGps.h>
#include "board_config.h"

static MotGps gps;

void setupL76kGps()
{
    Serial.println("Using external L76K GPS through MotGps");

#ifdef GPS_WAKEUP_PIN
    pinMode(GPS_WAKEUP_PIN, OUTPUT);
    digitalWrite(GPS_WAKEUP_PIN, HIGH);
#endif

    MotGpsConfig config;
    config.rxPin = GPS_RX_PIN;
    config.txPin = GPS_TX_PIN;
    config.baud = GPS_BAUD;
    config.serialPort = 2;
    config.validFixMaxAgeMs = 5000;
    config.setSystemTimeFromGps = true;

    gps.begin(config);
}

void l76kGpsLoop()
{
    gps.loop();
}

bool l76kGpsValid()
{
    return gps.valid();
}

double l76kLatitude()
{
    return gps.latitude();
}

double l76kLongitude()
{
    return gps.longitude();
}

double l76kSpeedKmph()
{
    return gps.speedKmph();
}

double l76kAltitudeMeters()
{
    return gps.altitudeMeters();
}

double l76kCourseDegrees()
{
    return gps.courseDegrees();
}

uint32_t l76kSatellites()
{
    return gps.satellites();
}

double l76kHdop()
{
    return gps.hdop();
}

uint32_t l76kLocationAgeMs()
{
    return gps.locationAgeMs();
}

String l76kGpsStatusJson()
{
    return gps.statusJson();
}
