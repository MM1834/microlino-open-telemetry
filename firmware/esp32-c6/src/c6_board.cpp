#include "c6_board.h"

#include <ESP.h>

void c6BoardSetup()
{
#ifdef MOT_XIAO_BOARD
    // Seeed XIAO ESP32-C6 RF switch: enable software control, then select the
    // ceramic antenna by default. Define MOT_XIAO_EXTERNAL_ANTENNA only for a
    // build whose target has a connected 2.4 GHz U.FL antenna.
    pinMode(3, OUTPUT);
    digitalWrite(3, LOW);
    delay(100);
    pinMode(14, OUTPUT);
#ifdef MOT_XIAO_EXTERNAL_ANTENNA
    digitalWrite(14, HIGH);
    Serial.println("WiFi antenna: external U.FL");
#else
    digitalWrite(14, LOW);
    Serial.println("WiFi antenna: internal ceramic");
#endif
#endif

    Serial.printf("Board: %s\n", MOT_BOARD);
    Serial.printf("Chip: %s rev %d, %u MHz\n",
                  ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz());
    Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
    Serial.printf("CAN1: RX GPIO%d / TX GPIO%d\n", MOT_CAN1_RX_PIN, MOT_CAN1_TX_PIN);
    Serial.printf("CAN2: RX GPIO%d / TX GPIO%d\n", MOT_CAN2_RX_PIN, MOT_CAN2_TX_PIN);
    Serial.printf("GPS:  RX GPIO%d / TX GPIO%d @ %d baud\n",
                  MOT_GPS_RX_PIN, MOT_GPS_TX_PIN, MOT_GPS_BAUD);
}
