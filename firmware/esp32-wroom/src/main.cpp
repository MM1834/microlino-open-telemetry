#include <Arduino.h>

#include "app_config.h"
#include "network/wifi_manager.h"
#include "mqtt/mqtt_client.h"
#include "web/web_ui.h"
#include "can/can_input.h"
#include "gps/wroom_gps.h"

#include "telemetry/telemetry.h"
#include "system/version.h"
#include "system/device_id.h"
#include "abrp/wroom_abrp.h"
#include "web/local_web_security.h"

static unsigned long lastMqttPublishMs = 0;
static unsigned long lastSystemUpdateMs = 0;
static String serialCommand;

static void pollSerialRecovery()
{
    while (Serial.available()) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\n' || character == '\r') {
            serialCommand.trim();
            if (serialCommand.equalsIgnoreCase("admin recover")) {
                config.otaPassword = LocalWebSecurity::generateRecoveryPassword();
                appConfigManager.save();
                Serial.println("Local admin user: admin");
                Serial.println("NEW LOCAL ADMIN PASSWORD: " + config.otaPassword);
                Serial.println("Local administration password replaced; reconnect to the WebUI");
            } else if (!serialCommand.isEmpty()) {
                Serial.println("Command: admin recover");
            }
            serialCommand = "";
        } else if (serialCommand.length() < 80) {
            serialCommand += character;
        }
    }
}

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

    appConfigManager.load();

    setupNetwork();
    setupMqtt();
    setupWroomAbrp();
    setupWebUi();
    setupCanInput();
    setupWroomGps();

    Serial.println("MOT setup ready");
    Serial.println("USB recovery: admin recover");
}

void loop()
{
    pollSerialRecovery();
    processCanInput();
    wroomGpsLoop();
    mqttLoop();
    webUiLoop();
    wroomAbrpLoop();

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
