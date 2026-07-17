#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

struct MotGpsConfig {
    int8_t rxPin = -1;        // ESP RX receives GNSS TX
    int8_t txPin = -1;        // ESP TX sends to GNSS RX
    uint32_t baud = 9600;
    uint8_t serialPort = 2;
    uint32_t validFixMaxAgeMs = 5000;
    bool setSystemTimeFromGps = false;
};

struct MotGpsStatus {
    bool started = false;
    bool seen = false;
    bool valid = false;
    bool timeValid = false;
    uint64_t chars = 0;
    uint32_t lastCharMs = 0;
    uint32_t ageMs = 0;
    uint32_t satellites = 0;
    double hdop = NAN;
    String message;
};

class MotGps {
public:
    MotGps();

    bool begin(const MotGpsConfig& config);
    void loop();

    bool started();
    bool seen();
    bool valid();
    bool dateTimeValid();

    double latitude();
    double longitude();
    double speedKmph();
    double altitudeMeters();
    double courseDegrees();
    uint32_t satellites();
    double hdop();
    uint32_t locationAgeMs();

    bool utcIso8601(char* buffer, size_t bufferSize);
    time_t utcEpoch();

    const MotGpsStatus& status();
    String statusJson();

private:
    HardwareSerial* serial_ = nullptr;
    TinyGPSPlus gps_;
    MotGpsConfig config_;
    MotGpsStatus status_;
    bool systemTimeSet_ = false;

    void refreshStatus();
    void updateSystemTime();
};
