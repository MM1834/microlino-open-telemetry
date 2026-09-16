#include "c6_status_led.h"

#include <Arduino.h>

#include "c6_aws.h"
#include "c6_config.h"
#include "c6_local_mqtt.h"
#include "c6_network.h"

#ifndef MOT_XIAO_BOARD
#include <esp32-hal-rgb-led.h>
#endif

namespace {
#ifndef MOT_XIAO_BOARD
constexpr uint8_t STATUS_LED_PIN = 8;
constexpr uint32_t STATUS_UPDATE_MS = 40;
constexpr uint32_t PULSE_PERIOD_MS = 2000;

struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr Color WIFI_CONNECTED = {0, 0, 14};
constexpr Color AWS_CONNECTED = {0, 14, 0};
constexpr Color AWS_AND_LOCAL_CONNECTED = {0, 14, 4};
constexpr Color LOCAL_MQTT_CONNECTED = {0, 10, 12};

uint32_t lastUpdateMs = 0;
Color lastColor = {255, 255, 255};
#endif
bool errorActive = false;

#ifndef MOT_XIAO_BOARD
uint8_t pulseBrightness(uint32_t now, uint8_t minimum, uint8_t maximum)
{
    const uint32_t halfPeriod = PULSE_PERIOD_MS / 2;
    const uint32_t position = now % PULSE_PERIOD_MS;
    const uint32_t ramp = position < halfPeriod ? position : PULSE_PERIOD_MS - position;
    return minimum + static_cast<uint8_t>(
        (static_cast<uint32_t>(maximum - minimum) * ramp) / halfPeriod);
}

Color desiredColor(uint32_t now)
{
    if (errorActive) {
        const uint8_t brightness = pulseBrightness(now, 2, 16);
        return {brightness, 0, 0};
    }
    if (!c6NetworkOnline()) {
        const uint8_t brightness = pulseBrightness(now, 2, 12);
        return {brightness, brightness, 0};
    }

    const bool awsConnected = c6AwsConnected();
    const bool localConnected = c6Config.localMqttEnabled && c6LocalMqttConnected();
    if (awsConnected && localConnected) return AWS_AND_LOCAL_CONNECTED;
    if (awsConnected) return AWS_CONNECTED;
    if (localConnected) return LOCAL_MQTT_CONNECTED;
    return WIFI_CONNECTED;
}

void writeColor(const Color &color)
{
    if (color.red == lastColor.red && color.green == lastColor.green &&
        color.blue == lastColor.blue) return;
    rgbLedWrite(STATUS_LED_PIN, color.red, color.green, color.blue);
    lastColor = color;
}
#endif
}

void c6StatusLedSetup()
{
#ifndef MOT_XIAO_BOARD
    // GPIO8 drives the nanoESP32-C6-N16 onboard WS2812. The XIAO build leaves
    // its board-specific LED and GPIO mapping untouched.
    writeColor({8, 8, 0});
#endif
}

void c6StatusLedLoop()
{
#ifndef MOT_XIAO_BOARD
    const uint32_t now = millis();
    if (now - lastUpdateMs < STATUS_UPDATE_MS) return;
    lastUpdateMs = now;
    writeColor(desiredColor(now));
#endif
}

void c6StatusLedSetError(bool active)
{
    errorActive = active;
}
