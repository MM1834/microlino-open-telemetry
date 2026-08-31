#include "lilygo_can.h"

#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>
#include "driver/twai.h"
#include "board_config.h"
#include "can/can_types.h"
#include "decoders/decoder_engine.h"
#include "config/lilygo_config.h"

namespace {

constexpr uint32_t CAN_BITRATE = 500000;
constexpr uint8_t MAX_FRAMES_PER_CHANNEL_PER_LOOP = 32;

struct ChannelStatus {
    bool ready = false;
    uint32_t framesRx = 0;
    uint32_t framesExt = 0;
    uint32_t framesStd = 0;
    uint32_t framesErr = 0;
    uint32_t framesRtr = 0;
    uint32_t suppressedDisplayCharging = 0;
    uint32_t lastFrameMs = 0;
    String lastError;
};

struct CanFrameLog {
    uint32_t ts = 0;
    uint32_t id = 0;
    uint8_t channel = 0;
    bool ext = false;
    bool rtr = false;
    uint8_t dlc = 0;
    uint8_t data[8] = {0};
};

ChannelStatus can1;
ChannelStatus can2;
MCP2515* can2Controller = nullptr;

constexpr size_t LOG_SIZE = 20;
CanFrameLog frameLog[LOG_SIZE];
size_t frameLogPos = 0;
size_t frameLogCount = 0;

String hexByte(uint8_t value)
{
    constexpr const char* HEX_DIGITS = "0123456789ABCDEF";
    String result;
    result += HEX_DIGITS[(value >> 4) & 0x0F];
    result += HEX_DIGITS[value & 0x0F];
    return result;
}

String esc(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

bool standardCanConfigured()
{
    return config.canProfile == DECODER_PROFILE_STANDARD_CAN_V1_PIONEER ||
           config.canProfile == DECODER_PROFILE_STANDARD_CAN_V2 ||
           config.can2Profile == DECODER_PROFILE_STANDARD_CAN_V1_PIONEER ||
           config.can2Profile == DECODER_PROFILE_STANDARD_CAN_V2;
}

void logFrame(uint8_t channel, const MotCanFrame& frame, bool rtr)
{
    CanFrameLog& item = frameLog[frameLogPos];
    item.ts = frame.receivedMs;
    item.id = frame.id;
    item.channel = channel;
    item.ext = frame.extended;
    item.rtr = rtr;
    item.dlc = min<uint8_t>(frame.dlc, 8);
    for (uint8_t i = 0; i < 8; ++i) item.data[i] = i < item.dlc ? frame.data[i] : 0;

    frameLogPos = (frameLogPos + 1) % LOG_SIZE;
    if (frameLogCount < LOG_SIZE) ++frameLogCount;
}

void accountFrame(ChannelStatus& status, uint8_t channel, MotCanFrame& frame, bool rtr,
                  DecoderProfile profile)
{
    ++status.framesRx;
    status.lastFrameMs = frame.receivedMs;
    if (frame.extended) ++status.framesExt;
    else ++status.framesStd;
    if (rtr) ++status.framesRtr;

    const bool displayChargingFrame = frame.id == 0x603 || frame.id == 0x604;
    const bool suppressDisplayCharging = standardCanConfigured() &&
        profile == DECODER_PROFILE_DISPLAY_CAN && displayChargingFrame;
    if (suppressDisplayCharging) {
        ++status.suppressedDisplayCharging;
    } else if (!rtr) {
        decoderEngineHandleFrame(frame, profile);
    }
    logFrame(channel, frame, rtr);
}

void setupCan1()
{
    Serial.printf("CAN1 setup: TWAI RX=%d TX=%d bitrate=500k, listen-only\n", CAN_RX_PIN, CAN_TX_PIN);
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(CAN_TX_PIN), static_cast<gpio_num_t>(CAN_RX_PIN), TWAI_MODE_LISTEN_ONLY);
    general.tx_queue_len = 0;
    general.rx_queue_len = 32;

    const twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t error = twai_driver_install(&general, &timing, &filter);
    if (error == ESP_OK) error = twai_start();
    if (error != ESP_OK) {
        can1.lastError = "TWAI setup failed: " + String(static_cast<int>(error));
        Serial.println(can1.lastError);
        twai_driver_uninstall();
        return;
    }

    can1.ready = true;
    Serial.printf("CAN1 started: %s (%s)\n", decoderProfileName(config.canProfile),
                  decoderProfileKey(config.canProfile));
}

void setupCan2()
{
    Serial.printf("CAN2 setup: MCP2515 SCK=%d MOSI=%d MISO=%d CS=%d INT=%d bitrate=500k, listen-only\n",
                  CAN2_SPI_SCK_PIN, CAN2_SPI_MOSI_PIN, CAN2_SPI_MISO_PIN, CAN2_SPI_CS_PIN, CAN2_INT_PIN);
    Serial.println("CAN2 hardware gate: Adafruit TERM must be open and SLNT must be tied to 3.3 V.");

    pinMode(CAN2_SPI_CS_PIN, OUTPUT);
    digitalWrite(CAN2_SPI_CS_PIN, HIGH);
    pinMode(CAN2_INT_PIN, INPUT);
    SPI.begin(CAN2_SPI_SCK_PIN, CAN2_SPI_MISO_PIN, CAN2_SPI_MOSI_PIN, CAN2_SPI_CS_PIN);

    // Construct only after Arduino and SPI are initialized. The library's
    // constructor configures GPIO and otherwise runs too early as a global.
    can2Controller = new MCP2515(CAN2_SPI_CS_PIN, 10000000, &SPI);
    MCP2515::ERROR error = can2Controller->reset();
    if (error == MCP2515::ERROR_OK) error = can2Controller->setBitrate(CAN_500KBPS, CAN2_MCP_CLOCK);
    if (error == MCP2515::ERROR_OK) error = can2Controller->setListenOnlyMode();
    if (error != MCP2515::ERROR_OK) {
        can2.lastError = "MCP2515 setup failed: " + String(static_cast<int>(error));
        Serial.println(can2.lastError);
        return;
    }

    can2.ready = true;
    Serial.printf("CAN2 started: %s (%s)\n", decoderProfileName(config.can2Profile),
                  decoderProfileKey(config.can2Profile));
}

String channelJson(uint8_t channel, const ChannelStatus& status, DecoderProfile profile)
{
    String json = "{";
    json += "\"channel\":" + String(channel) + ",";
    json += "\"ready\":" + String(status.ready ? "true" : "false") + ",";
    json += "\"profileId\":" + String(static_cast<int>(profile)) + ",";
    json += "\"profileKey\":\"" + String(decoderProfileKey(profile)) + "\",";
    json += "\"profileName\":\"" + String(decoderProfileName(profile)) + "\",";
    json += "\"profileImplemented\":" + String(decoderProfileImplemented(profile) ? "true" : "false") + ",";
    json += "\"framesRx\":" + String(status.framesRx) + ",";
    json += "\"framesStd\":" + String(status.framesStd) + ",";
    json += "\"framesExt\":" + String(status.framesExt) + ",";
    json += "\"framesRtr\":" + String(status.framesRtr) + ",";
    json += "\"suppressedDisplayCharging\":" + String(status.suppressedDisplayCharging) + ",";
    json += "\"busErrors\":" + String(status.framesErr) + ",";
    json += "\"lastFrameMs\":" + String(status.lastFrameMs) + ",";
    json += "\"ageMs\":" + String(status.lastFrameMs ? millis() - status.lastFrameMs : 0) + ",";
    json += "\"lastError\":\"" + esc(status.lastError) + "\"";
    json += "}";
    return json;
}

}  // namespace

void setupLilygoCan()
{
    setupCan1();
    setupCan2();
}

void lilygoCanLoop()
{
    if (can1.ready) {
        twai_message_t message;
        uint8_t processed = 0;
        while (processed++ < MAX_FRAMES_PER_CHANNEL_PER_LOOP && twai_receive(&message, 0) == ESP_OK) {
            MotCanFrame frame;
            frame.id = message.identifier;
            frame.extended = message.extd;
            frame.dlc = min<uint8_t>(message.data_length_code, 8);
            frame.receivedMs = millis();
            for (uint8_t i = 0; i < frame.dlc; ++i) frame.data[i] = message.data[i];
            accountFrame(can1, 1, frame, message.rtr, config.canProfile);
        }

        twai_status_info_t status;
        if (twai_get_status_info(&status) == ESP_OK) can1.framesErr = status.bus_error_count;
    }

    if (can2.ready) {
        struct can_frame message;
        uint8_t processed = 0;
        MCP2515::ERROR error = MCP2515::ERROR_OK;
        while (processed++ < MAX_FRAMES_PER_CHANNEL_PER_LOOP &&
               (error = can2Controller->readMessage(&message)) == MCP2515::ERROR_OK) {
            MotCanFrame frame;
            frame.id = message.can_id & (message.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
            frame.extended = (message.can_id & CAN_EFF_FLAG) != 0;
            frame.dlc = min<uint8_t>(message.can_dlc, 8);
            frame.receivedMs = millis();
            for (uint8_t i = 0; i < frame.dlc; ++i) frame.data[i] = message.data[i];
            accountFrame(can2, 2, frame, (message.can_id & CAN_RTR_FLAG) != 0, config.can2Profile);
        }
        if (error != MCP2515::ERROR_OK && error != MCP2515::ERROR_NOMSG) {
            ++can2.framesErr;
            can2.lastError = "MCP2515 receive error: " + String(static_cast<int>(error));
        }
    }
}

bool lilygoCanReady()
{
    return can1.ready || can2.ready;
}

String lilygoCanStatusJson()
{
    String json = "{";
    json += "\"ready\":" + String(lilygoCanReady() ? "true" : "false") + ",";
    json += "\"allReady\":" + String(can1.ready && can2.ready ? "true" : "false") + ",";
    json += "\"bitrate\":\"500k\",\"receiveOnly\":true,";
    json += "\"can1\":" + channelJson(1, can1, config.canProfile) + ",";
    json += "\"can2\":" + channelJson(2, can2, config.can2Profile) + ",";
    json += "\"hardware\":{";
    json += "\"can1\":{\"controller\":\"ESP32 TWAI\",\"rxPin\":" + String(CAN_RX_PIN) +
            ",\"txPin\":" + String(CAN_TX_PIN) + "},";
    json += "\"can2\":{\"controller\":\"MCP2515\",\"sckPin\":" + String(CAN2_SPI_SCK_PIN) +
            ",\"mosiPin\":" + String(CAN2_SPI_MOSI_PIN) + ",\"misoPin\":" + String(CAN2_SPI_MISO_PIN) +
            ",\"csPin\":" + String(CAN2_SPI_CS_PIN) + ",\"intPin\":" + String(CAN2_INT_PIN) +
            ",\"termOpenRequired\":true,\"slntHighRequired\":true}";
    json += "}}";
    return json;
}

String lilygoCanFramesJson()
{
    String json = "{\"frames\":[";
    for (size_t i = 0; i < frameLogCount; ++i) {
        const size_t index = (frameLogPos + LOG_SIZE - 1 - i) % LOG_SIZE;
        const CanFrameLog& frame = frameLog[index];
        if (i) json += ",";
        json += "{\"ts\":" + String(frame.ts) + ",";
        json += "\"ageMs\":" + String(millis() - frame.ts) + ",";
        json += "\"channel\":" + String(frame.channel) + ",";
        json += "\"id\":" + String(frame.id) + ",";
        json += "\"idHex\":\"0x" + String(frame.id, HEX) + "\",";
        json += "\"ext\":" + String(frame.ext ? "true" : "false") + ",";
        json += "\"rtr\":" + String(frame.rtr ? "true" : "false") + ",";
        json += "\"dlc\":" + String(frame.dlc) + ",\"data\":\"";
        for (uint8_t byte = 0; byte < frame.dlc; ++byte) {
            if (byte) json += " ";
            json += hexByte(frame.data[byte]);
        }
        json += "\"}";
    }
    json += "]}";
    return json;
}
