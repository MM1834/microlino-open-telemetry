#include "lilygo_can.h"

#include <Arduino.h>
#include "driver/twai.h"
#include "board_config.h"
#include "can/can_types.h"
#include "decoders/decoder_engine.h"

static bool canReadyFlag = false;
static uint32_t framesRx = 0;
static uint32_t framesExt = 0;
static uint32_t framesStd = 0;
static uint32_t framesErr = 0;
static uint32_t framesRtr = 0;
static uint32_t lastFrameMs = 0;
static String lastError = "";

struct CanFrameLog {
    uint32_t ts = 0;
    uint32_t id = 0;
    bool ext = false;
    bool rtr = false;
    uint8_t dlc = 0;
    uint8_t data[8] = {0};
};

static const size_t LOG_SIZE = 20;
static CanFrameLog frameLog[LOG_SIZE];
static size_t frameLogPos = 0;
static size_t frameLogCount = 0;

static String hexByte(uint8_t v)
{
    const char* h = "0123456789ABCDEF";
    String s;
    s += h[(v >> 4) & 0x0F];
    s += h[v & 0x0F];
    return s;
}

static String esc(String s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\r", "\\r");
    s.replace("\n", "\\n");
    return s;
}

static void logFrame(const twai_message_t& msg)
{
    CanFrameLog& f = frameLog[frameLogPos];
    f.ts = millis();
    f.id = msg.identifier;
    f.ext = msg.extd;
    f.rtr = msg.rtr;
    f.dlc = msg.data_length_code;
    for (uint8_t i = 0; i < 8; i++) {
        f.data[i] = i < msg.data_length_code ? msg.data[i] : 0;
    }

    frameLogPos = (frameLogPos + 1) % LOG_SIZE;
    if (frameLogCount < LOG_SIZE) frameLogCount++;
}

void setupLilygoCan()
{
    Serial.printf("CAN input setup: RX=%d TX=%d bitrate=500k\n", CAN_RX_PIN, CAN_TX_PIN);
    Serial.println("CAN mode: TWAI receive with decoder diagnostics. No application CAN frames are sent.");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        lastError = "twai_driver_install failed: " + String((int)err);
        Serial.println(lastError);
        canReadyFlag = false;
        return;
    }

    err = twai_start();
    if (err != ESP_OK) {
        lastError = "twai_start failed: " + String((int)err);
        Serial.println(lastError);
        twai_driver_uninstall();
        canReadyFlag = false;
        return;
    }

    canReadyFlag = true;
    lastError = "";
    Serial.println("CAN input started");
}

void lilygoCanLoop()
{
    if (!canReadyFlag) return;

    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) {
        framesRx++;
        lastFrameMs = millis();

        if (msg.extd) framesExt++;
        else framesStd++;

        if (msg.rtr) framesRtr++;

        MotCanFrame frame;
        frame.id = msg.identifier;
        frame.extended = msg.extd;
        frame.dlc = msg.data_length_code;
        frame.receivedMs = millis();
        for (uint8_t i = 0; i < frame.dlc && i < 8; ++i) {
            frame.data[i] = msg.data[i];
        }
        decoderEngineHandleFrame(frame);

        logFrame(msg);
    }

    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
        framesErr = status.bus_error_count;
    }
}

bool lilygoCanReady()
{
    return canReadyFlag;
}

String lilygoCanStatusJson()
{
    twai_status_info_t status;
    bool hasStatus = canReadyFlag && twai_get_status_info(&status) == ESP_OK;

    String json = "{";
    json += "\"ready\":" + String(canReadyFlag ? "true" : "false") + ",";
    json += "\"rxPin\":" + String(CAN_RX_PIN) + ",";
    json += "\"txPin\":" + String(CAN_TX_PIN) + ",";
    json += "\"bitrate\":\"500k\",";
    json += "\"framesRx\":" + String(framesRx) + ",";
    json += "\"framesStd\":" + String(framesStd) + ",";
    json += "\"framesExt\":" + String(framesExt) + ",";
    json += "\"framesRtr\":" + String(framesRtr) + ",";
    json += "\"busErrors\":" + String(framesErr) + ",";
    json += "\"lastFrameMs\":" + String(lastFrameMs) + ",";
    json += "\"ageMs\":" + String(lastFrameMs ? millis() - lastFrameMs : 0) + ",";
    if (hasStatus) {
        json += "\"state\":" + String((int)status.state) + ",";
        json += "\"msgsToRx\":" + String(status.msgs_to_rx) + ",";
        json += "\"msgsToTx\":" + String(status.msgs_to_tx) + ",";
        json += "\"txFailed\":" + String(status.tx_failed_count) + ",";
        json += "\"rxMissed\":" + String(status.rx_missed_count) + ",";
        json += "\"arbLost\":" + String(status.arb_lost_count) + ",";
    }
    json += "\"lastError\":\"" + esc(lastError) + "\"";
    json += "}";
    return json;
}

String lilygoCanFramesJson()
{
    String json = "{";
    json += "\"frames\":[";

    for (size_t i = 0; i < frameLogCount; i++) {
        size_t idx = (frameLogPos + LOG_SIZE - 1 - i) % LOG_SIZE;
        const CanFrameLog& f = frameLog[idx];

        if (i) json += ",";
        json += "{";
        json += "\"ts\":" + String(f.ts) + ",";
        json += "\"ageMs\":" + String(millis() - f.ts) + ",";
        json += "\"id\":" + String(f.id) + ",";
        json += "\"idHex\":\"0x" + String(f.id, HEX) + "\",";
        json += "\"ext\":" + String(f.ext ? "true" : "false") + ",";
        json += "\"rtr\":" + String(f.rtr ? "true" : "false") + ",";
        json += "\"dlc\":" + String(f.dlc) + ",";
        json += "\"data\":\"";
        for (uint8_t b = 0; b < f.dlc; b++) {
            if (b) json += " ";
            json += hexByte(f.data[b]);
        }
        json += "\"";
        json += "}";
    }

    json += "]}";
    return json;
}
