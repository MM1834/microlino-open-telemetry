#include "c6_telemetry_cadence.h"

#include <math.h>

#include "telemetry/telemetry.h"

namespace {
constexpr float MOVING_SPEED_KMH = 1.0f;
constexpr float POWER_PEAK_W = 12000.0f;
constexpr uint32_t DISPLAY_FRESH_MS = 3000;
constexpr uint32_t BMS_FRESH_MS = 10000;
constexpr uint32_t ACCESS_WAKE_MS = 60000;
constexpr uint32_t PARKED_FIVE_MIN_AFTER_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t PARKED_TEN_MIN_AFTER_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t CHARGING_INTERVAL_MS = 30000;
constexpr uint32_t PARKED_FIVE_MIN_INTERVAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t PARKED_TEN_MIN_INTERVAL_MS = 10UL * 60UL * 1000UL;

enum class Mode : uint8_t { Active, Awake, Charging, ParkedFive, ParkedTen };

struct ObservedState {
    bool initialized = false;
    bool moving = false;
    bool charging = false;
    bool pluggedValid = false;
    bool plugged = false;
    bool doorValid = false;
    bool doorOpen = false;
    bool lockValid = false;
    bool locked = false;
    bool powerPeak = false;
};

ObservedState observed;
uint32_t eventGeneration = 1;
uint32_t inactivitySinceMs = 0;
uint32_t wakeUntilMs = 0;
Mode mode = Mode::Active;

bool beforeDeadline(uint32_t nowMs, uint32_t deadlineMs)
{
    return deadlineMs != 0 && static_cast<int32_t>(deadlineMs - nowMs) > 0;
}

uint32_t atLeast(uint32_t configured, uint32_t minimum)
{
    return configured < minimum ? minimum : configured;
}
}

void c6TelemetryCadenceSetup()
{
    observed = ObservedState();
    eventGeneration = 1;
    inactivitySinceMs = millis();
    wakeUntilMs = 0;
    mode = Mode::Active;
}

void c6TelemetryCadenceUpdate()
{
    const uint32_t nowMs = millis();
    const bool displayFresh = telemetry.display.valid &&
        static_cast<uint32_t>(nowMs - telemetry.display.lastUpdateMs) <= DISPLAY_FRESH_MS;
    const bool bmsFresh = telemetry.bms.statusLastUpdateMs != 0 &&
        static_cast<uint32_t>(nowMs - telemetry.bms.statusLastUpdateMs) <= BMS_FRESH_MS;
    const bool currentFresh = telemetry.bms.packCurrentValid &&
        static_cast<uint32_t>(nowMs - telemetry.bms.packCurrentLastUpdateMs) <= BMS_FRESH_MS;
    const bool moving = displayFresh && isfinite(telemetry.display.speedKmh) &&
        telemetry.display.speedKmh > MOVING_SPEED_KMH;
    const bool chargingFresh = telemetry.charging.valid &&
        static_cast<uint32_t>(nowMs - telemetry.charging.lastUpdateMs) <= BMS_FRESH_MS;
    const bool charging = (bmsFresh || chargingFresh) && telemetryIsCharging();
    const bool pluggedValid = bmsFresh && telemetry.bms.packStatusValid;
    const bool plugged = pluggedValid && telemetry.bms.plugged;
    const bool powerPeak = currentFresh && isfinite(telemetry.bms.vehiclePowerW) &&
        fabsf(telemetry.bms.vehiclePowerW) >= POWER_PEAK_W;

    if (!observed.initialized) {
        observed.initialized = true;
        observed.moving = moving;
        observed.charging = charging;
        observed.pluggedValid = pluggedValid;
        observed.plugged = plugged;
        observed.doorValid = telemetry.access.doorOpenValid;
        observed.doorOpen = telemetry.access.doorOpen;
        observed.lockValid = telemetry.access.centralLockValid;
        observed.locked = telemetry.access.centralLocked;
        observed.powerPeak = powerPeak;
        inactivitySinceMs = moving ? 0 : nowMs;
    } else {
        const bool motionChanged = moving != observed.moving;
        const bool chargeChanged = charging != observed.charging;
        const bool pluggedChanged = pluggedValid != observed.pluggedValid ||
            (pluggedValid && plugged != observed.plugged);
        const bool doorChanged = telemetry.access.doorOpenValid != observed.doorValid ||
            (telemetry.access.doorOpenValid && telemetry.access.doorOpen != observed.doorOpen);
        const bool lockChanged = telemetry.access.centralLockValid != observed.lockValid ||
            (telemetry.access.centralLockValid &&
             telemetry.access.centralLocked != observed.locked);
        const bool peakStarted = powerPeak && !observed.powerPeak;
        if (motionChanged || chargeChanged || pluggedChanged || doorChanged ||
            lockChanged || peakStarted) {
            eventGeneration++;
        }
        if (chargeChanged || pluggedChanged || doorChanged || lockChanged || peakStarted)
            wakeUntilMs = nowMs + ACCESS_WAKE_MS;
        if (motionChanged) inactivitySinceMs = moving ? 0 : nowMs;
        observed.moving = moving;
        observed.charging = charging;
        observed.pluggedValid = pluggedValid;
        observed.plugged = plugged;
        observed.doorValid = telemetry.access.doorOpenValid;
        observed.doorOpen = telemetry.access.doorOpen;
        observed.lockValid = telemetry.access.centralLockValid;
        observed.locked = telemetry.access.centralLocked;
        observed.powerPeak = powerPeak;
    }

    if (moving) {
        mode = Mode::Active;
    } else if (charging) {
        mode = Mode::Charging;
    } else if (beforeDeadline(nowMs, wakeUntilMs)) {
        mode = Mode::Awake;
    } else {
        if (inactivitySinceMs == 0) inactivitySinceMs = nowMs;
        const uint32_t inactiveMs = static_cast<uint32_t>(nowMs - inactivitySinceMs);
        if (inactiveMs < PARKED_FIVE_MIN_AFTER_MS) mode = Mode::Active;
        else if (inactiveMs < PARKED_TEN_MIN_AFTER_MS) mode = Mode::ParkedFive;
        else mode = Mode::ParkedTen;
    }
}

uint32_t c6TelemetryCadenceEventGeneration() { return eventGeneration; }

uint32_t c6TelemetryCadenceIntervalMs(uint32_t activeIntervalMs)
{
    switch (mode) {
        case Mode::Charging: return atLeast(activeIntervalMs, CHARGING_INTERVAL_MS);
        case Mode::ParkedFive: return atLeast(activeIntervalMs, PARKED_FIVE_MIN_INTERVAL_MS);
        case Mode::ParkedTen: return atLeast(activeIntervalMs, PARKED_TEN_MIN_INTERVAL_MS);
        default: return activeIntervalMs;
    }
}

const char *c6TelemetryCadenceMode()
{
    switch (mode) {
        case Mode::Active: return "active";
        case Mode::Awake: return "event-awake";
        case Mode::Charging: return "charging";
        case Mode::ParkedFive: return "parked-5min";
        case Mode::ParkedTen: return "parked-10min";
    }
    return "unknown";
}
