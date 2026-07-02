#include "can_input.h"
#include "board_config.h"

#include <Arduino.h>
#include "driver/twai.h"
#include "can/can_types.h"
#include "decoders/decoder_engine.h"

static bool canStarted = false;

void setupCanInput()
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        TWAI_MODE_LISTEN_ONLY
    );

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        Serial.printf("CAN driver install failed: %d\n", err);
        return;
    }

    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("CAN start failed: %d\n", err);
        return;
    }

    canStarted = true;
    Serial.println("CAN input 1 started: Microlino Display CAN");
}

void processCanInput()
{
    if (!canStarted) return;

    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(5)) != ESP_OK) {
        return;
    }

    MotCanFrame frame;
    frame.id = msg.identifier;
    frame.extended = msg.extd;
    frame.dlc = msg.data_length_code;
    frame.receivedMs = millis();
    for (uint8_t i = 0; i < frame.dlc && i < 8; ++i) {
        frame.data[i] = msg.data[i];
    }

    decoderEngineHandleFrame(frame);
}
