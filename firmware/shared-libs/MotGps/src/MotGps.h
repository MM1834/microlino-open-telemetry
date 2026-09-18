#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

struct MotGpsConfig {
    int8_t rxPin = -1;        // ESP RX receives GNSS TX
    int8_t txPin = -1;        // ESP TX sends to GNSS RX
    uint32_t baud = 9600;
    uint8_t serialPort = 2;
    uint32_t validFixMaxAgeMs = 5000;
    uint8_t minAltitudeSatellites = 6;
    double maxAltitudeHdop = 5.0;
    bool setSystemTimeFromGps = false;
};

enum class MotGpsState : uint8_t {
    GPS_DISABLED = 0,
    GPS_NOT_DETECTED,
    GPS_DETECTED,
    GPS_FIX
};

struct MotGpsStatus {
    bool started = false;
    bool seen = false;          // raw UART bytes received
    bool detected = false;      // at least one checksum-valid NMEA sentence
    bool valid = false;
    MotGpsState state = MotGpsState::GPS_DISABLED;
    bool timeValid = false;
    uint64_t chars = 0;
    uint32_t lastCharMs = 0;
    uint32_t ageMs = 0;
    uint32_t satellites = 0;
    uint32_t satellitesAgeMs = 0;
    double hdop = NAN;
    uint32_t hdopAgeMs = 0;
    bool altitudeValid = false;
    uint32_t altitudeAgeMs = 0;
    String message;
};

class MotGps {
public:
    MotGps();

    bool begin(const MotGpsConfig& config);
    void loop();

    bool started();
    bool seen();
    bool detected();
    bool valid();
    MotGpsState state();
    const char* stateName();
    bool dateTimeValid();

    double latitude();
    double longitude();
    double speedKmph();
    bool altitudeValid();
    bool altitudePublishable(bool moving);
    double altitudeMeters();
    double courseDegrees();
    uint32_t satellites();
    double hdop();
    uint32_t locationAgeMs();

    bool utcIso8601(char* buffer, size_t bufferSize);
    time_t utcEpoch();

    const MotGpsStatus& status();
    String statusJson();
    const char* altitudeDecision();

private:
    HardwareSerial* serial_ = nullptr;
    TinyGPSPlus gps_;
    MotGpsConfig config_;
    MotGpsStatus status_;
    bool systemTimeSet_ = false;
    bool altitudeAnchorValid_ = false;
    double altitudeAnchorLatitude_ = NAN;
    double altitudeAnchorLongitude_ = NAN;
    double altitudeAnchorMeters_ = NAN;
    bool altitudeProvisionalValid_ = false;
    double altitudeProvisionalLatitude_ = NAN;
    double altitudeProvisionalLongitude_ = NAN;
    double altitudeProvisionalMeters_ = NAN;
    bool altitudeCandidateValid_ = false;
    double altitudeCandidateLatitude_ = NAN;
    double altitudeCandidateLongitude_ = NAN;
    double altitudeCandidateMeters_ = NAN;
    bool altitudeCandidateAccepted_ = false;
    uint32_t altitudeRejectedSamples_ = 0;
    String altitudeDecision_ = "not_evaluated";
    bool altitudeLastAcceptedValid_ = false;
    double altitudeLastAcceptedMeters_ = NAN;

    void refreshStatus();
    void updateSystemTime();
};
