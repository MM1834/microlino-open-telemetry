#include "l76k_gps.h"

#include <Arduino.h>
#include <MotGps.h>
#include "board_config.h"
#include "../config/lilygo_config.h"
#include "../modem/lilygo_modem.h"

static MotGps gps;
static uint32_t lastSetupAttemptMs = 0;

static bool startGps()
{
#if defined(LILYGO_SIM7670G_S3_STANDARD)
    lastSetupAttemptMs = millis();
    Serial.println("Using integrated SIM7670G GNSS NMEA through MotGps");
    if (!lilygoConfigureIntegratedGpsNmea(true)) {
        Serial.println("SIM7670G GNSS NMEA setup failed; retry scheduled");
        return false;
    }
#else
    Serial.println("Using external L76K GPS through MotGps");

#ifdef GPS_WAKEUP_PIN
    pinMode(GPS_WAKEUP_PIN, OUTPUT);
    digitalWrite(GPS_WAKEUP_PIN, HIGH);
#endif
#endif

    MotGpsConfig gpsConfig;
    gpsConfig.rxPin = GPS_RX_PIN;
    gpsConfig.txPin = GPS_TX_PIN;
    gpsConfig.baud = GPS_BAUD;
    gpsConfig.serialPort = 2;
    gpsConfig.validFixMaxAgeMs = 5000;
    gpsConfig.setSystemTimeFromGps = true;
    gps.begin(gpsConfig);
    return true;
}

void setupL76kGps()
{
    if (!config.gpsEnabled) {
#if defined(LILYGO_SIM7670G_S3_STANDARD)
        Serial.println("SIM7670G GNSS: disabled by configuration");
        lilygoConfigureIntegratedGpsNmea(false);
#else
        Serial.println("L76K GPS: disabled by configuration");
#ifdef GPS_WAKEUP_PIN
        pinMode(GPS_WAKEUP_PIN, OUTPUT);
        digitalWrite(GPS_WAKEUP_PIN, LOW);
#endif
#endif
        return;
    }

    startGps();
}

void l76kGpsLoop()
{
    if (!config.gpsEnabled) return;
#if defined(LILYGO_SIM7670G_S3_STANDARD)
    if (!gps.started()) {
        constexpr uint32_t RETRY_INTERVAL_MS = 30000;
        if (lastSetupAttemptMs == 0 || millis() - lastSetupAttemptMs >= RETRY_INTERVAL_MS) {
            startGps();
        }
        return;
    }
#endif
    gps.loop();
}

bool l76kGpsStarted()
{
    return gps.started();
}

bool l76kGpsDetected()
{
    return gps.detected();
}

const char* l76kGpsStateName()
{
    return gps.stateName();
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
