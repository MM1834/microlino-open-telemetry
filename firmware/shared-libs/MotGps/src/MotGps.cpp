#include "MotGps.h"

#include <ArduinoJson.h>
#include <sys/time.h>
#include <time.h>

namespace {
bool validDateTime(TinyGPSPlus& gps)
{
    if (!gps.date.isValid() || !gps.time.isValid()) return false;

    const int year = gps.date.year();
    return year >= 2024 &&
           gps.date.month() >= 1 && gps.date.month() <= 12 &&
           gps.date.day() >= 1 && gps.date.day() <= 31 &&
           gps.time.hour() <= 23 &&
           gps.time.minute() <= 59 &&
           gps.time.second() <= 60;
}

// Howard Hinnant's civil-date conversion, adapted for Unix epoch days.
// It avoids timegm(), which is not available in all ESP32 Arduino toolchains.
int64_t daysFromCivil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra =
        static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 +
        day - 1;
    const unsigned dayOfEra =
        yearOfEra * 365 + yearOfEra / 4 -
        yearOfEra / 100 + dayOfYear;

    return static_cast<int64_t>(era) * 146097 +
           static_cast<int64_t>(dayOfEra) - 719468;
}

time_t makeUtcEpoch(TinyGPSPlus& gps)
{
    if (!validDateTime(gps)) return 0;

    const int64_t days = daysFromCivil(
        gps.date.year(),
        gps.date.month(),
        gps.date.day()
    );

    const int64_t seconds =
        days * 86400LL +
        static_cast<int64_t>(gps.time.hour()) * 3600LL +
        static_cast<int64_t>(gps.time.minute()) * 60LL +
        static_cast<int64_t>(gps.time.second());

    return static_cast<time_t>(seconds);
}
}
MotGps::MotGps() = default;

bool MotGps::begin(const MotGpsConfig& config)
{
    config_ = config;
    status_ = MotGpsStatus();
    systemTimeSet_ = false;

    if (config_.rxPin < 0) {
        status_.message = "GNSS RX pin is not configured";
        return false;
    }

    if (config_.serialPort == 0) {
        serial_ = &Serial;
    } else if (config_.serialPort == 1) {
        serial_ = &Serial1;
    } else {
        serial_ = &Serial2;
    }

    serial_->begin(
        config_.baud,
        SERIAL_8N1,
        config_.rxPin,
        config_.txPin
    );

    status_.started = true;
    status_.message = "GNSS UART started";

    Serial.printf(
        "MotGps: UART%u RX=%d TX=%d baud=%lu\n",
        config_.serialPort,
        config_.rxPin,
        config_.txPin,
        static_cast<unsigned long>(config_.baud)
    );

    return true;
}

void MotGps::loop()
{
    if (!serial_) return;

    while (serial_->available()) {
        const char c = static_cast<char>(serial_->read());
        gps_.encode(c);
        status_.seen = true;
        status_.chars++;
        status_.lastCharMs = millis();
    }

    refreshStatus();

    if (config_.setSystemTimeFromGps && !systemTimeSet_) {
        updateSystemTime();
    }
}

void MotGps::refreshStatus()
{
    status_.timeValid = validDateTime(gps_);
    status_.ageMs = gps_.location.isValid()
        ? gps_.location.age()
        : 0;
    status_.valid =
        gps_.location.isValid() &&
        status_.ageMs <= config_.validFixMaxAgeMs;
    status_.satellites = satellites();
    status_.hdop = hdop();

    if (!status_.started) {
        status_.message = "GNSS not started";
    } else if (!status_.seen) {
        status_.message = "Waiting for NMEA data";
    } else if (!status_.valid) {
        status_.message = "NMEA received, waiting for fix";
    } else {
        status_.message = "GNSS fix valid";
    }
}

void MotGps::updateSystemTime()
{
    const time_t epoch = utcEpoch();
    if (epoch <= 0) return;

    timeval tv {};
    tv.tv_sec = epoch;
    tv.tv_usec = 0;

    if (settimeofday(&tv, nullptr) == 0) {
        systemTimeSet_ = true;
        Serial.printf(
            "MotGps: system UTC set from GNSS epoch=%lld\n",
            static_cast<long long>(epoch)
        );
    }
}

bool MotGps::started()
{
    return status_.started;
}

bool MotGps::seen()
{
    return status_.seen;
}

bool MotGps::valid()
{
    return status_.valid;
}

bool MotGps::dateTimeValid()
{
    return validDateTime(gps_);
}

double MotGps::latitude()
{
    return gps_.location.isValid() ? gps_.location.lat() : NAN;
}

double MotGps::longitude()
{
    return gps_.location.isValid() ? gps_.location.lng() : NAN;
}

double MotGps::speedKmph()
{
    return gps_.speed.isValid() ? gps_.speed.kmph() : NAN;
}

double MotGps::altitudeMeters()
{
    return gps_.altitude.isValid() ? gps_.altitude.meters() : NAN;
}

double MotGps::courseDegrees()
{
    return gps_.course.isValid() ? gps_.course.deg() : NAN;
}

uint32_t MotGps::satellites()
{
    return gps_.satellites.isValid()
        ? gps_.satellites.value()
        : 0;
}

double MotGps::hdop()
{
    return gps_.hdop.isValid() ? gps_.hdop.hdop() : NAN;
}

uint32_t MotGps::locationAgeMs()
{
    return gps_.location.isValid() ? gps_.location.age() : 0;
}

bool MotGps::utcIso8601(char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize < 21 || !dateTimeValid()) return false;

    snprintf(
        buffer,
        bufferSize,
        "%04d-%02d-%02dT%02d:%02d:%02dZ",
        gps_.date.year(),
        gps_.date.month(),
        gps_.date.day(),
        gps_.time.hour(),
        gps_.time.minute(),
        gps_.time.second()
    );
    return true;
}

time_t MotGps::utcEpoch()
{
    return makeUtcEpoch(gps_);
}

const MotGpsStatus& MotGps::status()
{
    refreshStatus();
    return status_;
}

String MotGps::statusJson()
{
    refreshStatus();

    JsonDocument doc;
    doc["started"] = status_.started;
    doc["seen"] = status_.seen;
    doc["valid"] = status_.valid;
    doc["timeValid"] = status_.timeValid;
    doc["chars"] = status_.chars;
    doc["lastCharMs"] = status_.lastCharMs;
    doc["ageMs"] = status_.ageMs;
    doc["satellites"] = status_.satellites;
    doc["message"] = status_.message;

    if (isnan(status_.hdop)) {
        doc["hdop"] = nullptr;
    } else {
        doc["hdop"] = status_.hdop;
    }

    if (gps_.location.isValid()) {
        doc["latitude"] = gps_.location.lat();
        doc["longitude"] = gps_.location.lng();
    } else {
        doc["latitude"] = nullptr;
        doc["longitude"] = nullptr;
    }

    if (gps_.speed.isValid()) {
        doc["speedKmph"] = gps_.speed.kmph();
    } else {
        doc["speedKmph"] = nullptr;
    }

    if (gps_.altitude.isValid()) {
        doc["altitudeMeters"] = gps_.altitude.meters();
    } else {
        doc["altitudeMeters"] = nullptr;
    }

    char utc[32];
    if (utcIso8601(utc, sizeof(utc))) {
        doc["utc"] = utc;
    } else {
        doc["utc"] = nullptr;
    }

    String result;
    serializeJson(doc, result);
    return result;
}
