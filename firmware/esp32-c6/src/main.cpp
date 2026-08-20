#include <Arduino.h>

#include "c6_board.h"
#include "c6_abrp.h"
#include "c6_aws.h"
#include "c6_can_scan.h"
#include "c6_drive_capture.h"
#include "c6_config.h"
#include "c6_dual_can.h"
#include "c6_gps.h"
#include "c6_network.h"
#include "c6_web.h"
#include "telemetry/telemetry.h"

namespace {
uint32_t lastStatusMs = 0;
String serialCommand;

DecoderProfile parseProfile(const String &value, bool &valid)
{
    valid = true;
    if (value == "display" || value == "display-can" || value == "0") {
        return DECODER_PROFILE_DISPLAY_CAN;
    }
    if (value == "v1" || value == "standard-can-v1-pioneer" || value == "2") {
        return DECODER_PROFILE_STANDARD_CAN_V1_PIONEER;
    }
    if (value == "v2" || value == "standard-can-v2" || value == "3") {
        return DECODER_PROFILE_STANDARD_CAN_V2;
    }
    if (value == "disabled" || value == "255") {
        return DECODER_PROFILE_DISABLED;
    }
    valid = false;
    return DECODER_PROFILE_DISABLED;
}

void printProfiles()
{
    Serial.printf("Profiles: CAN1=%s (%s), CAN2=%s (%s)\n",
                  decoderProfileName(c6CanStatus(0).profile),
                  decoderProfileKey(c6CanStatus(0).profile),
                  decoderProfileName(c6CanStatus(1).profile),
                  decoderProfileKey(c6CanStatus(1).profile));
}

void handleSerialCommand(String command)
{
    command.trim();
    String normalized = command;
    normalized.toLowerCase();
    if (normalized == "profiles") {
        printProfiles();
        return;
    }
    if (normalized == "scan reset") {
        c6CanScanReset();
        return;
    }
    if (normalized == "scan dump") {
        c6CanScanDump();
        return;
    }
    if (normalized == "drive reset") {
        c6DriveCaptureReset();
        return;
    }
    if (normalized == "drive dump") {
        c6DriveCaptureDump();
        return;
    }
    if (normalized == "drive trace") {
        c6DriveCaptureTraceDump();
        return;
    }

    if (normalized == "wifi status") {
        Serial.println("WiFi: " + c6NetworkStatus());
        Serial.printf("Profiles configured: home=%s mobile=%s\n",
                      c6NetworkHomeConfigured() ? "yes" : "no",
                      c6NetworkMobileConfigured() ? "yes" : "no");
        return;
    }
    if (normalized == "wifi clear") {
        c6ConfigClearWifi();
        Serial.println("Home WiFi configuration cleared; restart required");
        return;
    }
    if (normalized.startsWith("wifi set ")) {
        const String value = command.substring(9);
        const int separator = value.indexOf('|');
        if (separator <= 0 || !c6ConfigSetWifi(value.substring(0, separator), value.substring(separator + 1))) {
            Serial.println("Usage: wifi set <ssid>|<password> (home profile)");
        } else {
            Serial.println("Home WiFi credentials saved without echo; restart required");
        }
        return;
    }
    if (normalized == "wifi2 clear") {
        c6ConfigClearWifi2();
        Serial.println("Mobile WiFi configuration cleared; restart required");
        return;
    }
    if (normalized.startsWith("wifi2 set ")) {
        const String value = command.substring(10);
        const int separator = value.indexOf('|');
        if (separator <= 0 || !c6ConfigSetWifi2(value.substring(0, separator), value.substring(separator + 1))) {
            Serial.println("Usage: wifi2 set <ssid>|<password> (mobile profile)");
        } else {
            Serial.println("Mobile WiFi credentials saved without echo; restart required");
        }
        return;
    }
    if (normalized == "aws status") {
        Serial.println("AWS IoT: " + c6AwsStatus());
        return;
    }
    if (normalized == "abrp status") {
        Serial.println("ABRP: " + c6AbrpStatusJson());
        return;
    }
    if (normalized == "abrp send") {
        Serial.println(c6AbrpQueueTelemetry() ? "ABRP: test queued" : "ABRP: test not queued; check status");
        return;
    }
    if (normalized == "abrp enable" || normalized == "abrp disable") {
        const bool enabled = normalized == "abrp enable";
        c6ConfigSetAbrpEnabled(enabled);
        Serial.printf("ABRP: %s (saved; credentials unchanged)\n", enabled ? "enabled" : "disabled");
        return;
    }
    if (normalized == "setup status") {
        Serial.println("Setup AP: " + c6NetworkApSsid());
        if (c6ConfigAdminConfigured()) {
            Serial.println("Local administration: configured");
        } else {
            Serial.println("Local administration: first setup required");
            Serial.println("Setup user: setup");
            Serial.println("Setup password: " + c6Config.setupPassword);
        }
        return;
    }
    if (normalized == "admin recover") {
        Serial.println("Local admin user: admin");
        Serial.println("NEW LOCAL ADMIN PASSWORD: " + c6ConfigRecoverAdminPassword());
        Serial.println("Local administration password replaced; reconnect to the WebUI");
        return;
    }

    if (!normalized.startsWith("profile ")) {
        Serial.println("Commands: profiles | profile <1|2> <display|v1|v2|disabled> | scan reset | scan dump | drive reset | drive dump | drive trace | wifi status | wifi set <ssid>|<password> | wifi clear | wifi2 set <ssid>|<password> | wifi2 clear | setup status | admin recover | aws status | abrp status | abrp send | abrp enable | abrp disable");
        return;
    }

    const int separator = normalized.indexOf(' ', 8);
    if (separator < 0) {
        Serial.println("Usage: profile <1|2> <display|v1|v2|disabled>");
        return;
    }

    const int channelNumber = normalized.substring(8, separator).toInt();
    bool validProfile = false;
    const DecoderProfile profile = parseProfile(normalized.substring(separator + 1), validProfile);
    if ((channelNumber != 1 && channelNumber != 2) || !validProfile ||
        !c6ConfigSetCanProfile(static_cast<size_t>(channelNumber - 1), profile)) {
        Serial.println("Profile change rejected");
        return;
    }
    printProfiles();
}

void pollSerialCommands()
{
    while (Serial.available()) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\n' || character == '\r') {
            if (!serialCommand.isEmpty()) {
                handleSerialCommand(serialCommand);
                serialCommand = "";
            }
        } else if (serialCommand.length() < 80) {
            serialCommand += character;
        }
    }
}
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("MOT ESP32-C6 dual-CAN qualification firmware");
    c6BoardSetup();
    telemetryInit();
    c6ConfigLoad();

    if (!c6DualCanSetup()) {
        Serial.println("Dual-CAN startup incomplete");
    }
    c6GpsSetup();
    c6NetworkSetup();
    c6WebSetup();
    c6AwsSetup();
    c6AbrpSetup();
    c6DriveCaptureReset();
    Serial.println("Console: profiles | profile <1|2> <display|v1|v2|disabled> | wifi status | wifi/wifi2 set <ssid>|<password> | wifi/wifi2 clear | setup status | admin recover | aws status | abrp status | abrp send | abrp enable | abrp disable");
}

void loop()
{
    c6DualCanLoop();
    c6GpsLoop();
    c6NetworkLoop();
    c6WebLoop();
    c6AwsLoop();
    c6AbrpLoop();
    pollSerialCommands();

    if (millis() - lastStatusMs >= 5000) {
        lastStatusMs = millis();
        const C6CanChannelStatus &can1 = c6CanStatus(0);
        const C6CanChannelStatus &can2 = c6CanStatus(1);
        Serial.printf("CAN1 frames=%lu errors=%lu | CAN2 frames=%lu errors=%lu | GPS=%s\n",
                      static_cast<unsigned long>(can1.frames),
                      static_cast<unsigned long>(can1.receiveErrors),
                      static_cast<unsigned long>(can2.frames),
                      static_cast<unsigned long>(can2.receiveErrors),
                      c6GpsState().c_str());
        Serial.printf("GPS UART: seen=%s chars=%llu detected=%s fix=%s message=%s\n",
                      c6GpsSeen() ? "yes" : "no",
                      static_cast<unsigned long long>(c6GpsChars()),
                      c6GpsDetected() ? "yes" : "no",
                      c6GpsValid() ? "yes" : "no",
                      c6GpsMessage().c_str());
        Serial.printf("WiFi=%s | AWS=%s\n", c6NetworkStatus().c_str(), c6AwsStatus().c_str());

        const uint32_t packAgeMs = millis() - telemetry.bms.packStatusLastUpdateMs;
        const uint32_t cellsAgeMs = millis() - telemetry.bms.cellVoltagesLastUpdateMs;
        if (telemetry.bms.packStatusValid && packAgeMs <= 10000) {
            Serial.printf("Standard-CAN BMS: pack=%.3f V SOC-field=%u status=0x%02X plugged=%s age=%lu ms\n",
                          telemetry.bms.packVoltageMv / 1000.0,
                          static_cast<unsigned>(telemetry.bms.socPercent),
                          static_cast<unsigned>(telemetry.bms.statusByte),
                          telemetry.bms.plugged ? "yes" : "no",
                          static_cast<unsigned long>(packAgeMs));
        }
        const uint32_t currentAgeMs = millis() - telemetry.bms.packCurrentLastUpdateMs;
        if (telemetry.bms.packCurrentValid && currentAgeMs <= 10000) {
            Serial.printf("Standard-CAN current=%.1f A power=%.0f W raw=%d rejected=%lu age=%lu ms\n",
                          telemetry.bms.packCurrentA,
                          telemetry.bms.packPowerW,
                          telemetry.bms.packCurrentRaw,
                          static_cast<unsigned long>(telemetry.bms.rejectedCurrentSamples),
                          static_cast<unsigned long>(currentAgeMs));
        }
        if (telemetry.bms.cellVoltagesValid && cellsAgeMs <= 10000) {
            Serial.printf("Standard-CAN V2 cells: A=%u mV B=%u mV min=%u mV max=%u mV delta=%u mV age=%lu ms\n",
                          static_cast<unsigned>(telemetry.bms.cellVoltageAMv),
                          static_cast<unsigned>(telemetry.bms.cellVoltageBMv),
                          static_cast<unsigned>(telemetry.bms.minCellVoltageMv),
                          static_cast<unsigned>(telemetry.bms.maxCellVoltageMv),
                          static_cast<unsigned>(telemetry.bms.cellVoltageDeltaMv),
                          static_cast<unsigned long>(cellsAgeMs));
        }
    }
}
