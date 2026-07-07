#include <Arduino.h>
#include <WiFi.h>
#include "board_config.h"
#include "telemetry/telemetry.h"
#include "config/lilygo_config.h"
#include "modem/lilygo_modem.h"
#include "network/lilygo_network.h"
#include "gps/l76k_gps.h"
#include "web/lilygo_web.h"
#include "mqtt/lilygo_mqtt.h"
#include "abrp/lilygo_abrp.h"
#include "can/lilygo_can.h"

void setup(){Serial.begin(115200);delay(1000);Serial.println();Serial.println("========================================");Serial.println(MOT_NAME);Serial.printf("Version : %s\n",MOT_VERSION);Serial.printf("Board   : %s\n",MOT_BOARD);Serial.println("========================================");Serial.println("LilyGO integration cleanup: LTE + external L76K GPS + config/status/OTA");Serial.println("CAN input is enabled via SN65HVD230.");Serial.printf("Documented CAN pins: RX=%d TX=%d\n",CAN_RX_PIN,CAN_TX_PIN);Serial.println("CAN enabled via SN65HVD230: RX GPIO32, TX GPIO13. GPIO33 remains MODEM_RI.");loadLilygoConfig();setupLilygoModem();setupL76kGps();setupLilygoCan();setupLilygoNetwork();setupLilygoMqtt();setupLilygoAbrp();setup;setupLilygoWeb();Serial.println("LilyGO setup ready");} void loop(){
    static unsigned long lastSystemUpdateMs = 0;
lilygoModemLoop();lilygoNetworkLoop();l76kGpsLoop();lilygoMqttLoop();lilygoAbrpLoop();lilygoWebLoop();lilygoCanLoop();if (millis() - lastSystemUpdateMs > 1000) { lastSystemUpdateMs = millis(); telemetryUpdateSystemRuntime(); telemetry.system.networkMode = lilygoNetworkModeName(); telemetry.system.ipAddress = lilygoNetworkIp(); telemetry.system.wifiRssi = lilygoNetworkModeName() == "WiFi" ? WiFi.RSSI() : 0; }
    delay(2);} 
