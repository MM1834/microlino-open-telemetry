#include "mot_gesture.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t kAddress = 0x6B;
constexpr int kSda = 18;
constexpr int kScl = 8;
constexpr uint8_t kWhoAmI = 0x00;
constexpr uint8_t kCtrl1 = 0x02;
constexpr uint8_t kCtrl2 = 0x03;
constexpr uint8_t kCtrl5 = 0x06;
constexpr uint8_t kCtrl7 = 0x08;
constexpr uint8_t kCtrl8 = 0x09;
constexpr uint8_t kCtrl9 = 0x0A;
constexpr uint8_t kCal1L = 0x0B;
constexpr uint8_t kStatusInt = 0x2D;
constexpr uint8_t kStatus0 = 0x2E;
constexpr uint8_t kStatus1 = 0x2F;
constexpr uint8_t kAccelStart = 0x35;
constexpr uint8_t kTapStatus = 0x59;
constexpr uint8_t kConfigureTapCommand = 0x0C;
constexpr uint8_t kCommandDone = 1U << 7;
constexpr uint8_t kTapDetected = 1U << 1;
constexpr uint8_t kTapEngineEnabled = (1U << 7) | 1U;
constexpr uint32_t kCtrl9TimeoutMs = 250;
constexpr uint32_t kSampleIntervalMs = 4;
constexpr uint32_t kMinimumKnockGapMs = 120;
constexpr uint32_t kMaximumKnockGapMs = 420;
constexpr uint32_t kSwitchCooldownMs = 1000;
constexpr int32_t kRawJerkThreshold = 400;  // 0.098 g at ±8 g.
constexpr int32_t kQuietJerkThreshold = 180;
constexpr uint32_t kImpulseQuietMs = 24;
constexpr uint32_t kImpulseTimeoutMs = 80;
constexpr uint32_t kStaleSampleMs = 50;
constexpr uint32_t kTiltCalibrationMs = 1500;
constexpr uint32_t kTiltHoldMs = 700;
constexpr uint32_t kServiceTiltHoldMs = 3000;
constexpr float kTiltTriggerDegrees = 35.0F;
constexpr float kTiltReleaseDegrees = 15.0F;
constexpr float kTiltFilterAlpha = 0.08F;
constexpr int32_t kTiltMinimumPlanarRaw = 2200;  // Gravity projection >= 0.54 g.
constexpr int32_t kTiltMinimumGravityRaw = 2800;
constexpr int32_t kTiltMaximumGravityRaw = 5400;

// Tap-engine parameters at 250 Hz. The lower peak threshold is intentional for
// the damped metal enclosure; the quiet threshold still rejects normal motion.
constexpr uint8_t kPeakWindowSamples = 8;          // 32 ms
constexpr uint16_t kTapWindowSamples = 18;         // 72 ms quiet before tap two
constexpr uint16_t kDoubleTapWindowSamples = 150;  // 600 ms
constexpr uint8_t kAxisPriorityZxy = 4;
constexpr uint8_t kAlpha = 8;                      // 0.0625
constexpr uint8_t kGamma = 32;                     // 0.25
constexpr uint16_t kPeakMagnitudeThreshold = 180;  // 0.176 g^2
constexpr uint16_t kUndefinedMotionThreshold = 154; // 0.150 g

bool available = false;
bool rawInitialized = false;
int16_t previousX = 0;
int16_t previousY = 0;
int16_t previousZ = 0;
uint32_t lastSampleMs = 0;
uint32_t firstKnockMs = 0;
uint32_t lastKnockMs = 0;
uint32_t lastSwitchMs = 0;
uint8_t knockCount = 0;
bool impulseActive = false;
uint32_t impulseStartedMs = 0;
uint32_t impulseLastHighMs = 0;
int32_t impulsePeak = 0;
bool tiltFilterInitialized = false;
bool tiltCalibrated = false;
bool tiltNeedsRelease = false;
float filteredX = 0.0F;
float filteredY = 0.0F;
float filteredZ = 0.0F;
float neutralAngleRadians = 0.0F;
uint32_t tiltCalibrationStartedMs = 0;
uint32_t tiltCandidateStartedMs = 0;
int8_t tiltCandidateDirection = 0;

bool writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(kAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool writeRegisters(uint8_t reg, const uint8_t *data, size_t length)
{
    Wire.beginTransmission(kAddress);
    Wire.write(reg);
    Wire.write(data, length);
    return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t reg, uint8_t *data, size_t length)
{
    Wire.beginTransmission(kAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(kAddress, static_cast<uint8_t>(length)) != length) return false;
    for (size_t index = 0; index < length; ++index) data[index] = Wire.read();
    return true;
}

bool runCtrl9Command(uint8_t command)
{
    if (!writeRegister(kCtrl9, command)) return false;
    const uint32_t startedMs = millis();
    uint8_t status = 0;
    while (millis() - startedMs < kCtrl9TimeoutMs) {
        if (readRegisters(kStatusInt, &status, 1) && (status & kCommandDone)) {
            return writeRegister(kCtrl9, 0x00);
        }
        delay(1);
    }
    return false;
}

bool configureTapEngine()
{
    const uint8_t firstSet[8] = {
        kPeakWindowSamples,
        kAxisPriorityZxy,
        static_cast<uint8_t>(kTapWindowSamples & 0xFFU),
        static_cast<uint8_t>(kTapWindowSamples >> 8),
        static_cast<uint8_t>(kDoubleTapWindowSamples & 0xFFU),
        static_cast<uint8_t>(kDoubleTapWindowSamples >> 8),
        0x00,
        0x01,
    };
    const uint8_t secondSet[8] = {
        kAlpha,
        kGamma,
        static_cast<uint8_t>(kPeakMagnitudeThreshold & 0xFFU),
        static_cast<uint8_t>(kPeakMagnitudeThreshold >> 8),
        static_cast<uint8_t>(kUndefinedMotionThreshold & 0xFFU),
        static_cast<uint8_t>(kUndefinedMotionThreshold >> 8),
        0x00,
        0x02,
    };
    return writeRegisters(kCal1L, firstSet, sizeof(firstSet)) &&
        runCtrl9Command(kConfigureTapCommand) &&
        writeRegisters(kCal1L, secondSet, sizeof(secondSet)) &&
        runCtrl9Command(kConfigureTapCommand);
}

int16_t littleEndian(const uint8_t *data)
{
    return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                (static_cast<uint16_t>(data[1]) << 8));
}

float normalizedAngle(float angle)
{
    while (angle > PI) angle -= 2.0F * PI;
    while (angle < -PI) angle += 2.0F * PI;
    return angle;
}

MotGestureEvent classifyTilt(uint32_t nowMs, int16_t x, int16_t y, int16_t z,
                             int32_t jerk, bool serviceHoldMode)
{
    if (!tiltFilterInitialized) {
        filteredX = x;
        filteredY = y;
        filteredZ = z;
        tiltFilterInitialized = true;
        tiltCalibrationStartedMs = nowMs;
        return MotGestureEvent::None;
    }
    filteredX += kTiltFilterAlpha * (x - filteredX);
    filteredY += kTiltFilterAlpha * (y - filteredY);
    filteredZ += kTiltFilterAlpha * (z - filteredZ);

    const float planarSquared = filteredX * filteredX + filteredY * filteredY;
    const float gravitySquared = planarSquared + filteredZ * filteredZ;
    const bool plausible =
        planarSquared >= static_cast<float>(kTiltMinimumPlanarRaw) * kTiltMinimumPlanarRaw &&
        gravitySquared >= static_cast<float>(kTiltMinimumGravityRaw) * kTiltMinimumGravityRaw &&
        gravitySquared <= static_cast<float>(kTiltMaximumGravityRaw) * kTiltMaximumGravityRaw &&
        jerk < kRawJerkThreshold;
    if (!plausible) {
        // On the status page a short tilt is deliberately emitted only after
        // returning to neutral, so it cannot race the three-second service
        // hold. The physical return movement can briefly exceed the jerk
        // threshold; retain an already established candidate until a stable
        // neutral sample confirms navigation. Stale samples are still cleared
        // by motGesturePoll before reaching this function.
        if (serviceHoldMode && tiltCandidateDirection != 0) return MotGestureEvent::None;
        tiltCandidateDirection = 0;
        tiltCandidateStartedMs = 0;
        return MotGestureEvent::None;
    }

    const float angle = atan2f(filteredY, filteredX);
    if (!tiltCalibrated) {
        if (nowMs - tiltCalibrationStartedMs < kTiltCalibrationMs)
            return MotGestureEvent::None;
        neutralAngleRadians = angle;
        tiltCalibrated = true;
        Serial.printf("IMU: tilt neutral calibrated at %.1f deg\n",
                      neutralAngleRadians * 180.0F / PI);
        return MotGestureEvent::None;
    }

    const float deltaRadians = normalizedAngle(angle - neutralAngleRadians);
    const float deltaDegrees = deltaRadians * 180.0F / PI;
    if (tiltNeedsRelease) {
        if (fabsf(deltaDegrees) <= kTiltReleaseDegrees) {
            tiltNeedsRelease = false;
            Serial.println("IMU: tilt returned to neutral");
        }
        return MotGestureEvent::None;
    }
    if (fabsf(deltaDegrees) <= kTiltReleaseDegrees) {
        const int8_t releasedDirection = tiltCandidateDirection;
        const uint32_t heldMs = tiltCandidateStartedMs
            ? nowMs - tiltCandidateStartedMs : 0U;
        tiltCandidateDirection = 0;
        tiltCandidateStartedMs = 0;
        // Slowly follow small mounting-angle changes without chasing a deliberate turn.
        if (fabsf(deltaDegrees) < 8.0F)
            neutralAngleRadians = normalizedAngle(neutralAngleRadians + deltaRadians * 0.002F);
        if (serviceHoldMode && releasedDirection != 0 && heldMs >= kTiltHoldMs) {
            Serial.printf("IMU: short service-page tilt released after %lums -> %s\n",
                          static_cast<unsigned long>(heldMs),
                          releasedDirection > 0 ? "right" : "left");
            return releasedDirection > 0
                ? MotGestureEvent::TiltRight : MotGestureEvent::TiltLeft;
        }
        return MotGestureEvent::None;
    }

    if (fabsf(deltaDegrees) < kTiltTriggerDegrees) return MotGestureEvent::None;

    const int8_t direction = deltaDegrees > 0.0F ? 1 : -1;
    if (direction != tiltCandidateDirection) {
        tiltCandidateDirection = direction;
        tiltCandidateStartedMs = nowMs;
        return MotGestureEvent::None;
    }
    const uint32_t holdMs = serviceHoldMode ? kServiceTiltHoldMs : kTiltHoldMs;
    if (nowMs - tiltCandidateStartedMs < holdMs)
        return MotGestureEvent::None;

    tiltCandidateDirection = 0;
    tiltCandidateStartedMs = 0;
    tiltNeedsRelease = true;
    Serial.printf("IMU: held tilt %.1f deg -> %s\n", deltaDegrees,
                  direction > 0 ? "right" : "left");
    if (serviceHoldMode) {
        return direction > 0
            ? MotGestureEvent::ServiceTiltRight : MotGestureEvent::ServiceTiltLeft;
    }
    return direction > 0 ? MotGestureEvent::TiltRight : MotGestureEvent::TiltLeft;
}
}  // namespace

void motGestureSetup()
{
    Wire.begin(kSda, kScl);
    Wire.setClock(400000);
    uint8_t identity = 0;
    if (!readRegisters(kWhoAmI, &identity, 1) || identity != 0x05) {
        Serial.printf("IMU: QMI8658 not found (id=0x%02x)\n", identity);
        return;
    }

    // Configure the hardware tap engine while both sensors are disabled. The
    // manufacturer recommends an accelerometer ODR above 200 Hz for tap use.
    const bool configured = writeRegister(kCtrl7, 0x00) &&
        writeRegister(kCtrl8, 1U << 7) &&
        writeRegister(kCtrl1, 0x60) &&
        writeRegister(kCtrl2, 0x25) &&  // ±8 g, 250 Hz.
        writeRegister(kCtrl5, 0x00) &&
        configureTapEngine();
    available = configured && writeRegister(kCtrl8, kTapEngineEnabled) &&
        writeRegister(kCtrl7, 0x01);
    Serial.printf("IMU: QMI8658 hardware tap engine %s (250 Hz)\n",
                  available ? "ready" : "initialization failed");
}

MotGestureEvent motGesturePoll(bool serviceHoldMode)
{
    if (!available) return MotGestureEvent::None;
    const uint32_t nowMs = millis();
    // Keep the hardware event visible for diagnosis, but use the 250 Hz raw
    // edge detector as the portable decision path for board revisions whose
    // activity engine does not raise STATUS1.TAP.
    uint8_t activityStatus = 0;
    if (readRegisters(kStatus1, &activityStatus, 1) && (activityStatus & kTapDetected)) {
        uint8_t tap = 0;
        if (readRegisters(kTapStatus, &tap, 1)) {
            Serial.printf("IMU: hardware tap count=%u axis=%u polarity=%u\n",
                          tap & 0x03U, (tap >> 4) & 0x03U, (tap >> 7) & 0x01U);
        }
    }

    if (nowMs - lastSampleMs < kSampleIntervalMs) return MotGestureEvent::None;
    const bool sampleStale = lastSampleMs && nowMs - lastSampleMs > kStaleSampleMs;
    lastSampleMs = nowMs;
    uint8_t dataStatus = 0;
    uint8_t raw[6] = {};
    if (!readRegisters(kStatus0, &dataStatus, 1) || !(dataStatus & 0x01U) ||
        !readRegisters(kAccelStart, raw, sizeof(raw))) {
        return MotGestureEvent::None;
    }
    const int16_t x = littleEndian(&raw[0]);
    const int16_t y = littleEndian(&raw[2]);
    const int16_t z = littleEndian(&raw[4]);
    if (!rawInitialized) {
        previousX = x;
        previousY = y;
        previousZ = z;
        rawInitialized = true;
        return MotGestureEvent::None;
    }
    const int32_t jerk = max(max(abs(static_cast<int32_t>(x) - previousX),
                                abs(static_cast<int32_t>(y) - previousY)),
                             abs(static_cast<int32_t>(z) - previousZ));
    previousX = x;
    previousY = y;
    previousZ = z;
    if (sampleStale) {
        impulseActive = false;
        impulsePeak = 0;
        tiltCandidateDirection = 0;
        tiltCandidateStartedMs = 0;
        return MotGestureEvent::None;
    }
    const MotGestureEvent tilt = classifyTilt(
        nowMs, x, y, z, jerk, serviceHoldMode);
    if (tilt != MotGestureEvent::None) return tilt;
    return MotGestureEvent::None;
}

bool motGestureAvailable()
{
    return available;
}
