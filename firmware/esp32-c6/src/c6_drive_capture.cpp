#include "c6_drive_capture.h"

#include <Arduino.h>
#include <cstring>
#include <math.h>

namespace {
static constexpr uint16_t TRACE_CAPACITY = 600;
static constexpr uint32_t TRACE_INTERVAL_MS = 200;

struct DriveTraceSample {
    uint32_t elapsedMs = 0;
    int16_t currentRaw = 0;
    uint16_t packVoltageMv = 0;
    uint16_t speedDeciKmh = 0;
    uint8_t statusByte = 0;
    uint8_t powerDisplay = 0;
    bool currentPlausible = false;
};

struct ExtremeFrame {
    bool valid = false;
    int16_t currentRaw = 0;
    uint32_t receivedMs = 0;
    uint8_t data[8] = {0};
};

struct DriveCapture {
    uint32_t startedMs = 0;
    uint32_t standardFrames = 0;
    uint32_t displayFrames = 0;
    bool standard18dSeen = false;
    int16_t currentMin = 0;
    int16_t currentMax = 0;
    uint16_t packVoltageMinMv = 0;
    uint16_t packVoltageMaxMv = 0;
    uint8_t standardSocMin = 0;
    uint8_t standardSocMax = 0;
    uint8_t statusFirst = 0;
    uint8_t statusLast = 0;
    uint8_t statusOr = 0;
    uint8_t statusAnd = 0;
    ExtremeFrame currentMinFrame;
    ExtremeFrame currentMaxFrame;
    bool id37fSeen = false;
    uint16_t pedalLeMin = 0;
    uint16_t pedalLeMax = 0;
    bool display602Seen = false;
    float displaySocMin = 0;
    float displaySocMax = 0;
    float speedMaxKmh = 0;
    bool display603Seen = false;
    uint8_t powerDisplayMin = 0;
    uint8_t powerDisplayMax = 0;
    bool display604Seen = false;
    bool displayPluggedFirst = false;
    bool displayPluggedLast = false;
    int16_t latestCurrentRaw = 0;
    uint16_t latestPackVoltageMv = 0;
    uint8_t latestStatusByte = 0;
    uint8_t latestPowerDisplay = 0;
    bool latest18dValid = false;
    uint32_t lastTraceMs = 0;
    uint16_t traceCount = 0;
    uint16_t traceHead = 0;
};

DriveCapture capture;
DriveTraceSample traceSamples[TRACE_CAPACITY];

uint16_t readLe16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

void storeExtreme(ExtremeFrame &target, int16_t current, const MotCanFrame &frame)
{
    target.valid = true;
    target.currentRaw = current;
    target.receivedMs = frame.receivedMs;
    memcpy(target.data, frame.data, 8);
}

void printExtreme(const char *label, const ExtremeFrame &frame)
{
    if (!frame.valid) return;
    Serial.printf("DRIVE %s currentRaw=%d currentCandidate=%.1f A at=%lu ms payload=",
                  label, frame.currentRaw, frame.currentRaw * 0.3f,
                  static_cast<unsigned long>(frame.receivedMs));
    for (uint8_t index = 0; index < 8; ++index) {
        Serial.printf("%02X%s", frame.data[index], index < 7 ? " " : "\n");
    }
}

bool plausibleCurrent(int16_t raw, uint16_t voltageMv)
{
    if (voltageMv < 40000 || voltageMv > 65000) return false;
    const float powerW = raw * 0.3f * (voltageMv / 1000.0f);
    return fabsf(powerW) <= 15000.0f;
}

void appendTrace(float speedKmh, uint32_t now)
{
    if (!capture.latest18dValid || now - capture.lastTraceMs < TRACE_INTERVAL_MS) return;
    capture.lastTraceMs = now;
    DriveTraceSample &sample = traceSamples[capture.traceHead];
    sample.elapsedMs = now - capture.startedMs;
    sample.currentRaw = capture.latestCurrentRaw;
    sample.packVoltageMv = capture.latestPackVoltageMv;
    sample.speedDeciKmh = static_cast<uint16_t>(lroundf(speedKmh * 10.0f));
    sample.statusByte = capture.latestStatusByte;
    sample.powerDisplay = capture.latestPowerDisplay;
    sample.currentPlausible = plausibleCurrent(sample.currentRaw, sample.packVoltageMv);
    capture.traceHead = (capture.traceHead + 1) % TRACE_CAPACITY;
    if (capture.traceCount < TRACE_CAPACITY) capture.traceCount++;
}
}

void c6DriveCaptureReset()
{
    capture = DriveCapture();
    memset(traceSamples, 0, sizeof(traceSamples));
    capture.startedMs = millis();
    Serial.println("Drive capture reset and recording");
}

void c6DriveCaptureObserve(size_t channel, const MotCanFrame &frame)
{
    if (frame.extended || frame.dlc < 8) return;

    if (channel == 0) {
        capture.standardFrames++;
        if (frame.id == 0x18D) {
            const int16_t current = static_cast<int16_t>(readLe16(&frame.data[1]));
            const uint16_t voltage = readLe16(&frame.data[3]);
            capture.latestCurrentRaw = current;
            capture.latestPackVoltageMv = voltage;
            capture.latestStatusByte = frame.data[6];
            capture.latest18dValid = true;
            if (!capture.standard18dSeen) {
                capture.standard18dSeen = true;
                capture.currentMin = capture.currentMax = current;
                capture.packVoltageMinMv = capture.packVoltageMaxMv = voltage;
                capture.standardSocMin = capture.standardSocMax = frame.data[7];
                capture.statusFirst = frame.data[6];
                capture.statusOr = capture.statusAnd = frame.data[6];
                storeExtreme(capture.currentMinFrame, current, frame);
                storeExtreme(capture.currentMaxFrame, current, frame);
            } else {
                if (current < capture.currentMin) {
                    capture.currentMin = current;
                    storeExtreme(capture.currentMinFrame, current, frame);
                }
                if (current > capture.currentMax) {
                    capture.currentMax = current;
                    storeExtreme(capture.currentMaxFrame, current, frame);
                }
                capture.packVoltageMinMv = min(capture.packVoltageMinMv, voltage);
                capture.packVoltageMaxMv = max(capture.packVoltageMaxMv, voltage);
                capture.standardSocMin = min(capture.standardSocMin, frame.data[7]);
                capture.standardSocMax = max(capture.standardSocMax, frame.data[7]);
            }
            capture.statusLast = frame.data[6];
            capture.statusOr |= frame.data[6];
            capture.statusAnd &= frame.data[6];
        } else if (frame.id == 0x37F) {
            const uint16_t pedal = readLe16(&frame.data[2]);
            if (!capture.id37fSeen) {
                capture.id37fSeen = true;
                capture.pedalLeMin = capture.pedalLeMax = pedal;
            } else {
                capture.pedalLeMin = min(capture.pedalLeMin, pedal);
                capture.pedalLeMax = max(capture.pedalLeMax, pedal);
            }
        }
        return;
    }

    if (channel != 1) return;
    capture.displayFrames++;
    if (frame.id == 0x602) {
        const float soc = frame.data[0] / 2.0f;
        const float speed = frame.data[1] / 2.0f;
        if (!capture.display602Seen) {
            capture.display602Seen = true;
            capture.displaySocMin = capture.displaySocMax = soc;
        } else {
            capture.displaySocMin = min(capture.displaySocMin, soc);
            capture.displaySocMax = max(capture.displaySocMax, soc);
        }
        capture.speedMaxKmh = max(capture.speedMaxKmh, speed);
        appendTrace(speed, frame.receivedMs);
    } else if (frame.id == 0x603) {
        const uint8_t power = frame.data[4];
        capture.latestPowerDisplay = power;
        if (!capture.display603Seen) {
            capture.display603Seen = true;
            capture.powerDisplayMin = capture.powerDisplayMax = power;
        } else {
            capture.powerDisplayMin = min(capture.powerDisplayMin, power);
            capture.powerDisplayMax = max(capture.powerDisplayMax, power);
        }
    } else if (frame.id == 0x604) {
        const bool plugged = (frame.data[4] & 0x10) != 0;
        if (!capture.display604Seen) {
            capture.display604Seen = true;
            capture.displayPluggedFirst = plugged;
        }
        capture.displayPluggedLast = plugged;
    }
}

void c6DriveCaptureTraceDump()
{
    Serial.println("DRIVE_TRACE elapsed_ms,speed_kmh,current_raw,current_a,pack_voltage_v,pack_power_w,status,power_display,plausible");
    const uint16_t oldest = capture.traceCount < TRACE_CAPACITY ? 0 : capture.traceHead;
    for (uint16_t offset = 0; offset < capture.traceCount; ++offset) {
        const DriveTraceSample &sample = traceSamples[(oldest + offset) % TRACE_CAPACITY];
        const float currentA = sample.currentRaw * 0.3f;
        const float voltageV = sample.packVoltageMv / 1000.0f;
        Serial.printf("DRIVE_TRACE %lu,%.1f,%d,%.1f,%.3f,%.0f,0x%02X,%u,%s\n",
                      static_cast<unsigned long>(sample.elapsedMs),
                      sample.speedDeciKmh / 10.0f,
                      sample.currentRaw,
                      currentA,
                      voltageV,
                      currentA * voltageV,
                      sample.statusByte,
                      sample.powerDisplay,
                      sample.currentPlausible ? "yes" : "no");
    }
    Serial.printf("DRIVE_TRACE samples=%u capacity=%u interval_ms=%lu retained_ms=%lu\n",
                  capture.traceCount, TRACE_CAPACITY,
                  static_cast<unsigned long>(TRACE_INTERVAL_MS),
                  static_cast<unsigned long>(TRACE_CAPACITY * TRACE_INTERVAL_MS));
}

void c6DriveCaptureDump()
{
    Serial.printf("DRIVE duration=%lu ms standardFrames=%lu displayFrames=%lu\n",
                  static_cast<unsigned long>(millis() - capture.startedMs),
                  static_cast<unsigned long>(capture.standardFrames),
                  static_cast<unsigned long>(capture.displayFrames));
    if (capture.standard18dSeen) {
        Serial.printf("DRIVE 0x18D currentRaw=%d..%d candidate=%.1f..%.1f A pack=%u..%u mV standardSOC=%u..%u statusFirst=0x%02X statusLast=0x%02X statusAnd=0x%02X statusOr=0x%02X\n",
                      capture.currentMin, capture.currentMax,
                      capture.currentMin * 0.3f, capture.currentMax * 0.3f,
                      capture.packVoltageMinMv, capture.packVoltageMaxMv,
                      capture.standardSocMin, capture.standardSocMax,
                      capture.statusFirst, capture.statusLast,
                      capture.statusAnd, capture.statusOr);
        printExtreme("CURRENT_MIN", capture.currentMinFrame);
        printExtreme("CURRENT_MAX", capture.currentMaxFrame);
    } else {
        Serial.println("DRIVE 0x18D not seen");
    }
    if (capture.id37fSeen) {
        Serial.printf("DRIVE 0x37F data[2..3] LE=%u..%u\n", capture.pedalLeMin, capture.pedalLeMax);
    } else {
        Serial.println("DRIVE 0x37F not seen");
    }
    if (capture.display602Seen) {
        Serial.printf("DRIVE 0x602 displaySOC=%.1f..%.1f %% speedMax=%.1f km/h\n",
                      capture.displaySocMin, capture.displaySocMax, capture.speedMaxKmh);
    } else {
        Serial.println("DRIVE 0x602 not seen");
    }
    if (capture.display603Seen) {
        Serial.printf("DRIVE 0x603 powerDisplay=%u..%u\n",
                      capture.powerDisplayMin, capture.powerDisplayMax);
    } else {
        Serial.println("DRIVE 0x603 not seen");
    }
    if (capture.display604Seen) {
        Serial.printf("DRIVE 0x604 pluggedFirst=%s pluggedLast=%s\n",
                      capture.displayPluggedFirst ? "yes" : "no",
                      capture.displayPluggedLast ? "yes" : "no");
    } else {
        Serial.println("DRIVE 0x604 not seen");
    }
}
