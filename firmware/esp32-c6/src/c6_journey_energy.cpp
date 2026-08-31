#include "c6_journey_energy.h"

#include "telemetry/telemetry.h"

#ifdef MOT_JOURNEY_ENERGY_COUNTER

#include <MotAwsIot.h>
#include <esp_system.h>
#include <math.h>

namespace {
constexpr float MOVING_SPEED_KMH = 1.0f;
constexpr uint32_t DISPLAY_FRESH_MS = 3000;
constexpr uint32_t POWER_FRESH_MS = 3000;
constexpr uint32_t MAX_INTEGRATION_STEP_MS = 2000;
constexpr uint32_t JOURNEY_STOP_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t CHECKPOINT_MS = 60000;
constexpr double WATT_MS_PER_WH = 3600000.0;

struct JourneyEnergyState {
    bool active = false;
    bool moving = false;
    bool sealed = false;
    bool publishPending = false;
    uint32_t bootNonce = 0;
    uint32_t journeySequence = 0;
    uint32_t stoppedSinceMs = 0;
    uint32_t lastIntegrationMs = 0;
    uint32_t lastPublishMs = 0;
    double drawnWattMs = 0.0;
    double regenWattMs = 0.0;
    char counterId[40] = {};
};

JourneyEnergyState state;

bool elapsedAtLeast(uint32_t nowMs, uint32_t sinceMs, uint32_t intervalMs)
{
    return sinceMs != 0 && static_cast<uint32_t>(nowMs - sinceMs) >= intervalMs;
}

void startJourney(uint32_t nowMs)
{
    state.active = true;
    state.sealed = false;
    state.publishPending = true;
    state.journeySequence++;
    state.stoppedSinceMs = 0;
    state.lastIntegrationMs = 0;
    state.lastPublishMs = 0;
    state.drawnWattMs = 0.0;
    state.regenWattMs = 0.0;
    snprintf(
        state.counterId,
        sizeof(state.counterId),
        "%08lx-%lu",
        static_cast<unsigned long>(state.bootNonce),
        static_cast<unsigned long>(state.journeySequence));
    Serial.printf("Journey energy: started id=%s at=%lu ms\n",
                  state.counterId, static_cast<unsigned long>(nowMs));
}

long drawnWh()
{
    return static_cast<long>(floor(state.drawnWattMs / WATT_MS_PER_WH));
}

long regenWh()
{
    return static_cast<long>(floor(state.regenWattMs / WATT_MS_PER_WH));
}
}

void c6JourneyEnergySetup()
{
    state = JourneyEnergyState();
    state.bootNonce = esp_random();
    Serial.println("Journey energy: N16 RAM counter enabled");
}

void c6JourneyEnergyLoop()
{
    const uint32_t nowMs = millis();
    const bool displayFresh = telemetry.display.valid &&
        static_cast<uint32_t>(nowMs - telemetry.display.lastUpdateMs) <= DISPLAY_FRESH_MS;
    const bool movingNow = displayFresh && isfinite(telemetry.display.speedKmh) &&
        telemetry.display.speedKmh > MOVING_SPEED_KMH;

    if (movingNow && (!state.active || state.sealed ||
        elapsedAtLeast(nowMs, state.stoppedSinceMs, JOURNEY_STOP_MS))) {
        startJourney(nowMs);
    }

    if (state.active && state.moving && !movingNow && state.stoppedSinceMs == 0) {
        state.stoppedSinceMs = nowMs;
        state.lastIntegrationMs = 0;
        state.publishPending = true;
        Serial.printf("Journey energy: stopped id=%s drawn=%ld Wh regen=%ld Wh\n",
                      state.counterId, drawnWh(), regenWh());
    } else if (state.active && movingNow) {
        state.stoppedSinceMs = 0;
    }

    const bool freshPower = telemetry.bms.packCurrentValid &&
        isfinite(telemetry.bms.vehiclePowerW) &&
        static_cast<uint32_t>(nowMs - telemetry.bms.packCurrentLastUpdateMs) <= POWER_FRESH_MS;
    if (state.active && movingNow && freshPower) {
        if (state.lastIntegrationMs != 0) {
            const uint32_t stepMs = static_cast<uint32_t>(nowMs - state.lastIntegrationMs);
            if (stepMs <= MAX_INTEGRATION_STEP_MS) {
                const double wattMs = static_cast<double>(telemetry.bms.vehiclePowerW) * stepMs;
                if (wattMs >= 0.0) state.drawnWattMs += wattMs;
                else state.regenWattMs -= wattMs;
            }
        }
        state.lastIntegrationMs = nowMs;
    } else {
        state.lastIntegrationMs = 0;
    }

    const bool freshPlugState = telemetry.bms.statusLastUpdateMs != 0 &&
        static_cast<uint32_t>(nowMs - telemetry.bms.statusLastUpdateMs) <= POWER_FRESH_MS;
    if (state.active && !state.sealed && !movingNow &&
        freshPlugState && telemetry.bms.plugged) {
        state.sealed = true;
        state.publishPending = true;
    }
    state.moving = movingNow;
}

bool c6JourneyEnergyPublish(MotAwsIotClient &client)
{
    if (!state.active || state.counterId[0] == '\0') return false;
    const uint32_t nowMs = millis();
    if (!state.publishPending && state.lastPublishMs != 0 &&
        !elapsedAtLeast(nowMs, state.lastPublishMs, CHECKPOINT_MS)) return false;

    const bool idOk = client.publish(
        "journey/energy_counter_id", String(state.counterId), false);
    const bool drawnOk = client.publishInt(
        "journey/energy_drawn_wh", drawnWh(), false);
    const bool regenOk = client.publishInt(
        "journey/energy_regen_wh", regenWh(), false);
    if (idOk && drawnOk && regenOk) {
        state.publishPending = false;
        state.lastPublishMs = nowMs;
    }
    return idOk || drawnOk || regenOk;
}

String c6JourneyEnergyStatusJson()
{
    String out = "{\"enabled\":true,\"active\":";
    out += state.active ? "true" : "false";
    out += ",\"moving\":" + String(state.moving ? "true" : "false");
    out += ",\"sealed\":" + String(state.sealed ? "true" : "false");
    out += ",\"counterId\":\"" + String(state.counterId) + "\"";
    out += ",\"drawnWh\":" + String(drawnWh());
    out += ",\"regenWh\":" + String(regenWh());
    out += "}";
    return out;
}

#else

void c6JourneyEnergySetup() {}
void c6JourneyEnergyLoop() {}
bool c6JourneyEnergyPublish(MotAwsIotClient &) { return false; }
String c6JourneyEnergyStatusJson() { return "{\"enabled\":false}"; }

#endif
