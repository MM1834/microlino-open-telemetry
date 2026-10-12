#include "c6_cloud_worker.h"

#include "c6_aws.h"
#include "c6_local_mqtt.h"

namespace {
TaskHandle_t workerTask = nullptr;
portMUX_TYPE workerStateMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t completedCycles = 0;
uint32_t serviceStartedMs = 0;
uint32_t lastAwsDurationMs = 0;
uint32_t lastLocalMqttDurationMs = 0;
uint32_t maximumAwsDurationMs = 0;
uint32_t maximumLocalMqttDurationMs = 0;
uint8_t activeService = 0;
volatile bool abrpPauseRequested = false;
volatile bool abrpPauseReady = false;

void beginService(uint8_t service)
{
    portENTER_CRITICAL(&workerStateMux);
    serviceStartedMs = millis();
    activeService = service;
    portEXIT_CRITICAL(&workerStateMux);
}

void finishService(uint8_t service)
{
    const uint32_t durationMs = millis() - serviceStartedMs;
    portENTER_CRITICAL(&workerStateMux);
    if (service == 1) {
        lastAwsDurationMs = durationMs;
        if (durationMs > maximumAwsDurationMs) maximumAwsDurationMs = durationMs;
    } else {
        lastLocalMqttDurationMs = durationMs;
        if (durationMs > maximumLocalMqttDurationMs)
            maximumLocalMqttDurationMs = durationMs;
    }
    activeService = 0;
    serviceStartedMs = 0;
    portEXIT_CRITICAL(&workerStateMux);
}

void cloudWorker(void *)
{
    for (;;) {
        if (abrpPauseRequested) {
            if (!abrpPauseReady) {
                c6AwsDisconnect();
                c6LocalMqttDisconnect();
                abrpPauseReady = true;
                Serial.println("Cloud worker: TLS transports paused for ABRP");
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        abrpPauseReady = false;
        if (c6LocalMqttNeedsExclusiveTlsWindow()) {
            c6AwsDisconnect();
            beginService(2);
            c6LocalMqttLoop();
            finishService(2);
        } else if (!c6LocalMqttExclusiveTlsCooldownActive()) {
            beginService(1);
            c6AwsLoop();
            finishService(1);

            if (!c6LocalMqttUsesExclusiveTlsWindows()) {
                beginService(2);
                c6LocalMqttLoop();
                finishService(2);
            }
        }

        portENTER_CRITICAL(&workerStateMux);
        completedCycles = completedCycles + 1U;
        portEXIT_CRITICAL(&workerStateMux);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

}

void c6CloudWorkerRequestAbrpPause() { abrpPauseRequested = true; }
bool c6CloudWorkerAbrpPauseReady() { return abrpPauseReady; }
void c6CloudWorkerReleaseAbrpPause()
{
    abrpPauseRequested = false;
    abrpPauseReady = false;
    Serial.println("Cloud worker: TLS transports released after ABRP");
}

void c6CloudWorkerSetup()
{
    if (workerTask) return;
    if (xTaskCreate(cloudWorker, "mot-cloud", 12288, nullptr, 1,
                    &workerTask) != pdPASS) {
        workerTask = nullptr;
        Serial.println("Cloud worker: task creation failed");
        return;
    }
    Serial.println("Cloud worker: AWS and local MQTT isolated from main loop");
}

void c6CloudWorkerLoop()
{
    static uint32_t lastBusyLogMs = 0;
    uint8_t service = 0;
    uint32_t startedMs = 0;
    portENTER_CRITICAL(&workerStateMux);
    service = activeService;
    startedMs = serviceStartedMs;
    portEXIT_CRITICAL(&workerStateMux);
    if (!service || !startedMs) return;
    const uint32_t nowMs = millis();
    const uint32_t busyMs = nowMs - startedMs;
    if (busyMs >= 1000U && nowMs - lastBusyLogMs >= 5000U) {
        lastBusyLogMs = nowMs;
        Serial.printf("Cloud worker busy: service=%s durationMs=%lu\n",
                      service == 1 ? "aws" : "local-mqtt",
                      static_cast<unsigned long>(busyMs));
    }
}

String c6CloudWorkerStatusJson()
{
    uint8_t service = 0;
    uint32_t startedMs = 0;
    uint32_t cycles = 0;
    uint32_t lastAwsMs = 0;
    uint32_t maxAwsMs = 0;
    uint32_t lastLocalMs = 0;
    uint32_t maxLocalMs = 0;
    portENTER_CRITICAL(&workerStateMux);
    service = activeService;
    startedMs = serviceStartedMs;
    cycles = completedCycles;
    lastAwsMs = lastAwsDurationMs;
    maxAwsMs = maximumAwsDurationMs;
    lastLocalMs = lastLocalMqttDurationMs;
    maxLocalMs = maximumLocalMqttDurationMs;
    portEXIT_CRITICAL(&workerStateMux);
    const uint32_t busyMs = service && startedMs ? millis() - startedMs : 0;
    return "{\"running\":" + String(workerTask ? "true" : "false") +
        ",\"activeService\":\"" +
        String(service == 1 ? "aws" : service == 2 ? "local-mqtt" : "idle") +
        "\",\"busyMs\":" + String(busyMs) +
        ",\"cycles\":" + String(cycles) +
        ",\"lastAwsMs\":" + String(lastAwsMs) +
        ",\"maxAwsMs\":" + String(maxAwsMs) +
        ",\"lastLocalMqttMs\":" + String(lastLocalMs) +
        ",\"maxLocalMqttMs\":" + String(maxLocalMs) + "}";
}
