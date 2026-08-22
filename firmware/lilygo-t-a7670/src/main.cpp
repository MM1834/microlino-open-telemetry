#include <Arduino.h>
#include <WiFi.h>

#include "board_config.h"
#include "telemetry/telemetry.h"
#include "system/device_id.h"
#include "config/lilygo_config.h"
#include "modem/lilygo_modem.h"
#include "network/lilygo_network.h"
#include "gps/l76k_gps.h"
#include "web/lilygo_web.h"
#include "mqtt/lilygo_mqtt.h"
#include "abrp/lilygo_abrp.h"
#include "can/lilygo_can.h"
#include "web/local_web_security.h"
#include "cache/lilygo_offline_cache.h"

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
                lilygoConfigManager.save();
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
    Serial.printf("Board   : %s\n", MOT_BOARD);
    Serial.println("========================================");
    Serial.printf("CAN: RX GPIO%d, TX GPIO%d\n", CAN_RX_PIN, CAN_TX_PIN);

    lilygoConfigManager.load();
    lilygoOfflineCacheSetup();
    setupLilygoModem();
    setupL76kGps();
    setupLilygoCan();
    setupLilygoNetwork();
    setupLilygoMqtt();
    setupLilygoAbrp();
    setupLilygoWeb();

    Serial.println("LilyGO setup ready");
    Serial.println("USB recovery: admin recover");
}

void loop()
{
    pollSerialRecovery();
    lilygoModemLoop();
    lilygoNetworkLoop();
    l76kGpsLoop();
    lilygoMqttLoop();
    lilygoAbrpLoop();
    lilygoWebLoop();
    lilygoCanLoop();

    if (millis() - lastSystemUpdateMs > 1000) {
        lastSystemUpdateMs = millis();
        telemetryUpdateSystemRuntime();
        telemetry.system.networkMode = lilygoNetworkModeName();
        telemetry.system.ipAddress = lilygoNetworkIp();
        telemetry.system.wifiRssi = lilygoNetworkModeName() == "WiFi" ? WiFi.RSSI() : 0;
    }

    delay(2);
}
