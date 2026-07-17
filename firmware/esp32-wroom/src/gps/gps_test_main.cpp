#include <Arduino.h>
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

static MotGps gps;
static uint32_t lastStatusMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("MOT GPS-1 ESP32-WROOM hardware test");
    Serial.printf(
        "ESP RX GPIO%d <- RAK12501 TX\n",
        MOT_GPS_RX_PIN
    );
    Serial.printf(
        "ESP TX GPIO%d -> RAK12501 RX\n",
        MOT_GPS_TX_PIN
    );
    Serial.printf("GNSS baud: %d\n", MOT_GPS_BAUD);
    Serial.println("========================================");

    MotGpsConfig config;
    config.rxPin = MOT_GPS_RX_PIN;
    config.txPin = MOT_GPS_TX_PIN;
    config.baud = MOT_GPS_BAUD;
    config.serialPort = 2;
    config.validFixMaxAgeMs = 5000;
    config.setSystemTimeFromGps = true;

    if (!gps.begin(config)) {
        Serial.println("MotGps begin failed");
    }
}

void loop()
{
    gps.loop();

    if (millis() - lastStatusMs >= 2000) {
        lastStatusMs = millis();

        Serial.println(gps.statusJson());

        if (!gps.seen()) {
            Serial.println(
                "CHECK: power, common GND, crossed RX/TX, "
                "9600 baud and antenna"
            );
        } else if (!gps.valid()) {
            Serial.println(
                "NMEA is arriving. Move antenna outdoors "
                "and wait for satellite fix."
            );
        } else {
            Serial.printf(
                "FIX lat=%.6f lon=%.6f speed=%.1f km/h "
                "sat=%lu hdop=%.1f age=%lu ms\n",
                gps.latitude(),
                gps.longitude(),
                gps.speedKmph(),
                static_cast<unsigned long>(gps.satellites()),
                gps.hdop(),
                static_cast<unsigned long>(gps.locationAgeMs())
            );
        }
    }

    delay(2);
}
