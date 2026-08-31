#include "c6_dual_can.h"

#include "driver/twai.h"
#include "can/can_types.h"
#include "c6_config.h"
#include "c6_can_scan.h"
#include "c6_drive_capture.h"
#include "decoders/decoder_engine.h"

namespace {

struct Channel {
    twai_handle_t handle = nullptr;
    C6CanChannelStatus status;
};

Channel channels[2];

bool standardCanConfigured()
{
    return channels[0].status.profile == DECODER_PROFILE_STANDARD_CAN_V1_PIONEER ||
           channels[0].status.profile == DECODER_PROFILE_STANDARD_CAN_V2 ||
           channels[1].status.profile == DECODER_PROFILE_STANDARD_CAN_V1_PIONEER ||
           channels[1].status.profile == DECODER_PROFILE_STANDARD_CAN_V2;
}

bool startChannel(size_t index, int controller, int rxPin, int txPin, DecoderProfile profile)
{
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(txPin),
        static_cast<gpio_num_t>(rxPin),
        TWAI_MODE_LISTEN_ONLY);
    general.controller_id = controller;
    general.tx_queue_len = 0;
    general.rx_queue_len = 32;

    twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t result = twai_driver_install_v2(
        &general, &timing, &filter, &channels[index].handle);
    if (result != ESP_OK) {
        Serial.printf("CAN%u install failed: %s (%d)\n",
                      static_cast<unsigned>(index + 1), esp_err_to_name(result), result);
        return false;
    }

    result = twai_start_v2(channels[index].handle);
    if (result != ESP_OK) {
        Serial.printf("CAN%u start failed: %s (%d)\n",
                      static_cast<unsigned>(index + 1), esp_err_to_name(result), result);
        twai_driver_uninstall_v2(channels[index].handle);
        channels[index].handle = nullptr;
        return false;
    }

    channels[index].status.started = true;
    channels[index].status.profile = profile;
    Serial.printf("CAN%u started: controller=%d RX=%d TX=%d profile=%s\n",
                  static_cast<unsigned>(index + 1), controller, rxPin, txPin,
                  decoderProfileName(profile));
    return true;
}

void receiveChannel(size_t index)
{
    Channel &channel = channels[index];
    if (!channel.status.started || channel.handle == nullptr) return;

    twai_message_t message{};
    esp_err_t result = twai_receive_v2(channel.handle, &message, 0);
    if (result == ESP_ERR_TIMEOUT) return;
    if (result != ESP_OK) {
        channel.status.receiveErrors++;
        return;
    }

    channel.status.frames++;
    channel.status.lastFrameMs = millis();

    MotCanFrame frame{};
    frame.id = message.identifier;
    frame.extended = message.extd;
    frame.dlc = message.data_length_code;
    frame.receivedMs = channel.status.lastFrameMs;
    for (uint8_t i = 0; i < frame.dlc && i < sizeof(frame.data); ++i) {
        frame.data[i] = message.data[i];
    }
    c6CanScanObserve(index, frame);
    c6DriveCaptureObserve(index, frame);
    const bool displayChargingFrame = frame.id == 0x603 || frame.id == 0x604;
    if (!(standardCanConfigured() &&
          channel.status.profile == DECODER_PROFILE_DISPLAY_CAN &&
          displayChargingFrame)) {
        decoderEngineHandleFrame(frame, channel.status.profile);
    }
}

}  // namespace

bool c6DualCanSetup()
{
    channels[0].status.profile = c6Config.can1Profile;
    channels[1].status.profile = c6Config.can2Profile;

    bool can1 = startChannel(0, 0, MOT_CAN1_RX_PIN, MOT_CAN1_TX_PIN,
                             channels[0].status.profile);
    bool can2 = startChannel(1, 1, MOT_CAN2_RX_PIN, MOT_CAN2_TX_PIN,
                             channels[1].status.profile);
    return can1 && can2;
}

bool c6DualCanSetProfile(size_t channel, DecoderProfile profile)
{
    if (channel >= 2 || decoderProfileFind(profile) == nullptr) {
        return false;
    }

    channels[channel].status.profile = profile;
    Serial.printf("CAN%u profile changed: %s (%s)\n",
                  static_cast<unsigned>(channel + 1),
                  decoderProfileName(profile),
                  decoderProfileKey(profile));
    return true;
}

void c6DualCanLoop()
{
    receiveChannel(0);
    receiveChannel(1);
}

const C6CanChannelStatus &c6CanStatus(size_t channel)
{
    return channels[channel < 2 ? channel : 0].status;
}
