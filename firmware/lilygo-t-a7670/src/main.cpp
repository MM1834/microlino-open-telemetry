#include <Arduino.h>
#include "board_config.h"
#include "config/lilygo_config.h"
#include "modem/lilygo_modem.h"
#include "network/lilygo_network.h"
#include "gps/l76k_gps.h"
#include "web/lilygo_web.h"
void setup(){Serial.begin(115200);delay(1000);Serial.println();Serial.println("========================================");Serial.println(MOT_NAME);Serial.printf("Version : %s\n",MOT_VERSION);Serial.printf("Board   : %s\n",MOT_BOARD);Serial.println("========================================");Serial.println("LilyGO integration cleanup: LTE + external L76K GPS + config/status/OTA");Serial.println("CAN is intentionally not started in this sprint.");Serial.printf("Documented CAN pins: RX=%d TX=%d\n",CAN_RX_PIN,CAN_TX_PIN);Serial.println("NOTE: GPIO33 is MODEM_RI_PIN; CAN TX pin must be reviewed before enabling CAN.");loadLilygoConfig();setupLilygoModem();setupL76kGps();setupLilygoNetwork();setupLilygoWeb();Serial.println("LilyGO setup ready");} void loop(){lilygoModemLoop();lilygoNetworkLoop();l76kGpsLoop();lilygoWebLoop();delay(2);} 
