#include "l76k_gps.h"

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "board_config.h"

static HardwareSerial SerialGPS(2);
static TinyGPSPlus gps;

static bool gpsSeen = false;
static unsigned long lastCharMs = 0;
static unsigned long charsTotal = 0;

void setupL76kGps()
{
    Serial.println("Using external L76K GPS");
    Serial.printf("GPS Serial2 RX=%d TX=%d baud=%d PPS=%d WAKEUP=%d\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD, GPS_PPS_PIN, GPS_WAKEUP_PIN);

#ifdef GPS_WAKEUP_PIN
    pinMode(GPS_WAKEUP_PIN, OUTPUT);
    digitalWrite(GPS_WAKEUP_PIN, HIGH);
#endif

    SerialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void l76kGpsLoop()
{
    while (SerialGPS.available()) {
        char c = (char)SerialGPS.read();
        gps.encode(c);
        gpsSeen = true;
        charsTotal++;
        lastCharMs = millis();
    }
}

bool l76kGpsValid()
{
    return gps.location.isValid();
}

double l76kLatitude()
{
    return gps.location.isValid() ? gps.location.lat() : NAN;
}

double l76kLongitude()
{
    return gps.location.isValid() ? gps.location.lng() : NAN;
}

double l76kSpeedKmph()
{
    return gps.speed.isValid() ? gps.speed.kmph() : NAN;
}

String l76kGpsStatusJson()
{
    String json = "{";
    json += "\"seen\":" + String(gpsSeen ? "true" : "false") + ",";
    json += "\"valid\":" + String(gps.location.isValid() ? "true" : "false") + ",";
    json += "\"ageMs\":" + String(gps.location.isValid() ? gps.location.age() : 0) + ",";
    json += "\"chars\":" + String(charsTotal) + ",";
    json += "\"lastCharMs\":" + String(lastCharMs) + ",";
    json += "\"satellites\":" + String(gps.satellites.isValid() ? gps.satellites.value() : 0) + ",";
    json += "\"hdop\":" + String(gps.hdop.isValid() ? gps.hdop.hdop() : 0, 1) + ",";

    if (gps.location.isValid()) {
        json += "\"lat\":" + String(gps.location.lat(), 6) + ",";
        json += "\"lon\":" + String(gps.location.lng(), 6) + ",";
    } else {
        json += "\"lat\":null,";
        json += "\"lon\":null,";
    }

    if (gps.speed.isValid()) {
        json += "\"speedKmph\":" + String(gps.speed.kmph(), 1) + ",";
    } else {
        json += "\"speedKmph\":null,";
    }

    if (gps.date.isValid() && gps.time.isValid()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 gps.date.year(), gps.date.month(), gps.date.day(),
                 gps.time.hour(), gps.time.minute(), gps.time.second());
        json += "\"utc\":\"" + String(buf) + "\"";
    } else {
        json += "\"utc\":null";
    }

    json += "}";
    return json;
}
