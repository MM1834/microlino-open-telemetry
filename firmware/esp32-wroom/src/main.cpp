#include <Arduino.h>

#include "app_config.h"
#include "network/wifi_manager.h"
#include "mqtt/mqtt_client.h"
#include "web/web_ui.h"
#include "can/can_input.h"

#include "telemetry/telemetry.h"
#include "system/version.h"
#include "system/device_id.h"
#include "common/abrp/abrp_client.h"

static unsigned long lastMqttPublishMs = 0;
static unsigned long lastSystemUpdateMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    telemetryInit();
    telemetry.system.firmwareVersion = MOT_VERSION;
    telemetry.system.deviceId = motDeviceId();

    Serial.println();
    Serial.println("========================================");
    Serial.println(MOT_NAME);
    Serial.printf("Version : %s\n", MOT_VERSION);
    Serial.printf("Device  : %s\n", telemetry.system.deviceId.c_str());
    Serial.println("========================================");

    loadConfig();

    setupNetwork();
    setupMqtt();
    setupAbrp();
    setupWebUi();
    setupCanInput();

    Serial.println("MOT setup ready");
}

void loop()
{
    processCanInput();
    mqttLoop();
    webUiLoop();
    abrpLoop();

    if (millis() - lastSystemUpdateMs > 1000) {
        lastSystemUpdateMs = millis();
        telemetryUpdateSystemRuntime();
        telemetry.system.wifiRssi = networkRssi();
        telemetry.system.ipAddress = networkIp();
        telemetry.system.networkMode = networkModeName();
    }

    if (millis() - lastMqttPublishMs > config.publishIntervalMs) {
        lastMqttPublishMs = millis();
        publishTelemetry();
    }
}
