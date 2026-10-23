#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <time.h>

#include "c6_board.h"
#include "c6_abrp.h"
#include "c6_aws.h"
#include "c6_charge_journal.h"
#include "c6_cloud_worker.h"
#include "c6_bm2_diag.h"
#include "c6_can_scan.h"
#include "c6_drive_capture.h"
#include "c6_soc_discovery.h"
#include "c6_vcu_diagnostic.h"
#include "c6_config.h"
#include "c6_data_volume.h"
#include "c6_dual_can.h"
#include "c6_display_ble.h"
#include "c6_gps.h"
#include "c6_journey_energy.h"
#include "c6_lock_discovery.h"
#include "c6_local_mqtt.h"
#include "c6_network.h"
#include "c6_offline_cache.h"
#include "c6_status_led.h"
#include "c6_telemetry_cadence.h"
#include "system/device_id.h"
#include "system/version.h"
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
    if (value == "v1" || value == "standard-can-v1-pioneer" ||
        value == "standard-can-v1-pioneer-gen1-midrange" || value == "2") {
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

String serialSafeWifiName(const String &value)
{
    if (value.isEmpty()) return "not-configured";
    String safe;
    safe.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value.charAt(i);
        safe += (c >= 32 && c != 127 && c != '"') ? c : ' ';
    }
    return safe;
}

const char *supportResetReason()
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

void printSupportCanStatus(size_t channel)
{
    const C6CanChannelStatus &status = c6CanStatus(channel);
    Serial.printf("Support CAN%u: started=%s profile=%s frames=%lu errors=%lu lastFrameAgeMs=",
                  static_cast<unsigned>(channel + 1), status.started ? "yes" : "no",
                  decoderProfileKey(status.profile), static_cast<unsigned long>(status.frames),
                  static_cast<unsigned long>(status.receiveErrors));
    if (status.frames) Serial.println(static_cast<unsigned long>(millis() - status.lastFrameMs));
    else Serial.println("never");
}

void printSupportStatus()
{
    const time_t now = time(nullptr);
    Serial.printf("Support system: firmware=%s device=%s board=%s uptimeSec=%lu reset=%s freeHeap=%u minFreeHeap=%u largestBlock=%u\n",
                  MOT_VERSION, motDeviceId().c_str(), MOT_BOARD,
                  static_cast<unsigned long>(millis() / 1000UL), supportResetReason(),
                  ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("Support network: state=%s profile=%s transportReady=%s ip=%s rssi=%d channel=%d weak=%s transitions=%lu disconnects=%lu lastDisconnect=%s lastDisconnectAgeMs=%lu managerInitiated=%s\n",
                  c6NetworkStateName().c_str(), c6NetworkProfileName().c_str(),
                  c6NetworkTransportReady() ? "yes" : "no", c6NetworkIp().c_str(),
                  c6NetworkRssi(), c6NetworkChannel(), c6NetworkLinkWeak() ? "yes" : "no",
                  static_cast<unsigned long>(c6NetworkTransitionCount()),
                  static_cast<unsigned long>(c6NetworkDisconnectCount()),
                  c6NetworkLastDisconnectReasonName().c_str(),
                  static_cast<unsigned long>(c6NetworkLastDisconnectAgeMs()),
                  c6NetworkLastDisconnectWasManagerInitiated() ? "yes" : "no");
    Serial.println("Support AWS: " + c6AwsStatus());
    Serial.println("Support cache: " + c6OfflineCacheStatusJson());
    Serial.printf("Support time: valid=%s epoch=%lld\n",
                  now >= 1700000000 ? "yes" : "no", static_cast<long long>(now));
    printSupportCanStatus(0);
    printSupportCanStatus(1);
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
    if (normalized == "support status") {
        printSupportStatus();
        return;
    }
    if (normalized == "bm2 scan") {
        if (!c6Bm2DiagStartScan()) Serial.println("BM2 scan not started (not available or already running)");
        return;
    }
    if (normalized == "bm2 stop") {
        c6Bm2DiagStopScan();
        return;
    }
    if (normalized == "bm2 status") {
        c6Bm2DiagPrintStatus();
        return;
    }
    if (normalized == "bm2 connect") {
        if (!c6Bm2DiagConnect()) Serial.println("BM2 GATT connection was not established");
        return;
    }
    if (normalized == "bm6 connect") {
        if (!c6Bm6DiagConnect()) Serial.println("BM6 GATT connection was not established");
        return;
    }
    if (normalized == "bm2 disconnect") {
        c6Bm2DiagDisconnect();
        return;
    }
    if (normalized == "bm2 history") {
        if (!c6Bm2DiagRequestHistory()) Serial.println("BM2 history request was not started");
        return;
    }
    if (normalized == "bm2 history dump") {
        c6Bm2DiagDumpHistory();
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
    if (normalized == "soc reset") {
        c6SocDiscoveryReset();
        return;
    }
    if (normalized == "soc mark") {
        c6SocDiscoveryMark();
        return;
    }
    if (normalized == "soc dump") {
        c6SocDiscoveryDump();
        return;
    }
    if (normalized == "soc diff") {
        c6SocDiscoveryDiff();
        return;
    }
    if (normalized == "vcu reset") {
        c6VcuDiagnosticReset();
        return;
    }
    if (normalized == "vcu dump") {
        c6VcuDiagnosticDump();
        return;
    }
    if (normalized == "version search") {
        c6LockDiscoveryVersionSearch();
        return;
    }
    if (normalized == "soc 48d") {
        c6SocDiscovery48dDump();
        return;
    }
    if (normalized == "lock reset") {
        c6LockDiscoveryReset();
        return;
    }
    if (normalized == "lock start") {
        c6LockDiscoveryStart();
        return;
    }
    if (normalized == "lock stop") {
        c6LockDiscoveryStop();
        return;
    }
    if (normalized == "lock dump") {
        c6LockDiscoveryDump();
        return;
    }
    if (normalized == "lock diff") {
        c6LockDiscoveryDiff();
        return;
    }

    if (normalized == "wifi status") {
        Serial.println("WiFi: " + c6NetworkStatus());
        Serial.printf("Profiles configured: home=%s mobile=%s\n",
                      c6NetworkHomeConfigured() ? "yes" : "no",
                      c6NetworkMobileConfigured() ? "yes" : "no");
        Serial.printf("WiFi profiles: home=\"%s\" mobile=\"%s\"\n",
                      serialSafeWifiName(c6Config.wifiSsid).c_str(),
                      serialSafeWifiName(c6Config.wifi2Ssid).c_str());
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
    if (normalized == "aws enable" || normalized == "aws disable") {
        const bool enabled = normalized == "aws enable";
        c6ConfigSetMotCloudEnabled(enabled);
        Serial.printf("MOT Cloud: %s (saved; credentials unchanged; restart required to enable)\n", enabled ? "enabled" : "disabled");
        return;
    }
    if (normalized == "cache status") {
        Serial.println("Offline cache: " + c6OfflineCacheStatusJson());
        return;
    }
    if (normalized == "cache enable" || normalized == "cache disable") {
        const bool enabled = normalized == "cache enable";
        c6ConfigSetOfflineCacheEnabled(enabled);
        Serial.printf("Offline cache: %s\n", enabled ? "enabled" : "disabled and purged");
        return;
    }
    if (normalized == "cache purge") {
        c6OfflineCachePurge();
        Serial.println("Offline cache: queued samples purged");
        return;
    }
    if (normalized == "energy status") {
        Serial.println("Journey energy: " + c6JourneyEnergyStatusJson());
        return;
    }
    if (normalized == "abrp status") {
        Serial.println("ABRP: " + c6AbrpStatusJson());
        return;
    }
    if (normalized == "mqtt status") {
        Serial.println("Local MQTT: " + c6LocalMqttStatusJson());
        return;
    }
    if (normalized == "volume status") {
        Serial.println("Data volume: " + c6DataVolumeStatusJson());
        return;
    }
    if (normalized == "volume reset") {
        Serial.println(c6DataVolumeReset()
            ? "Data volume: counters reset and checkpointed"
            : "Data volume: analysis build not enabled or reset failed");
        return;
    }
    if (normalized == "mqtt enable" || normalized == "mqtt disable") {
        const bool enabled = normalized == "mqtt enable";
        c6ConfigSetLocalMqttEnabled(enabled);
        Serial.printf("Local MQTT: %s (credentials unchanged)\n", enabled ? "enabled" : "disabled");
        return;
    }
    if (normalized == "abrp clear") {
        c6ConfigClearAbrpCredentials();
        Serial.println("ABRP: disabled; API key and user token deleted");
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
        Serial.println("Commands: support status | profiles | profile <1|2> <display|v1|v2|disabled> | bm2 scan | bm2 stop | bm2 status | bm2 connect | bm6 connect | bm2 disconnect | bm2 history | bm2 history dump | scan reset | scan dump | drive reset | drive dump | drive trace | soc reset | soc mark | soc dump | soc diff | soc 48d | vcu reset | vcu dump | version search | lock reset | lock start | lock stop | lock dump | lock diff | wifi status | wifi set <ssid>|<password> | wifi clear | wifi2 set <ssid>|<password> | wifi2 clear | setup status | admin recover | aws status | aws enable | aws disable | mqtt status | mqtt enable | mqtt disable | volume status | volume reset | cache status | cache enable | cache disable | cache purge | energy status | abrp status | abrp send | abrp enable | abrp disable | abrp clear");
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
    c6StatusLedSetup();
    telemetryInit();
    c6ConfigLoad();
    c6OfflineCacheSetup();
    c6JourneyEnergySetup();
    c6ChargeJournalSetup();
    c6TelemetryCadenceSetup();

    if (!c6DualCanSetup()) {
        Serial.println("Dual-CAN startup incomplete");
        c6StatusLedSetError(true);
    }
    c6GpsSetup();
    c6NetworkSetup();
    c6WebSetup();
    c6AwsSetup();
    c6LocalMqttSetup();
    c6AbrpSetup();
    c6DataVolumeSetup();
    c6DisplayBleSetup();
#ifdef MOT_DISPLAY_BLE
    c6CloudWorkerSetup();
#endif
    c6Bm2DiagSetup();
    c6DriveCaptureReset();
    c6LockDiscoveryReset();
    Serial.println("Console: support status | profiles | profile <1|2> <display|v1|v2|disabled> | bm2 scan | bm2 stop | bm2 status | bm2 connect | bm6 connect | bm2 disconnect | bm2 history | bm2 history dump | wifi status | wifi/wifi2 set <ssid>|<password> | wifi/wifi2 clear | setup status | admin recover | aws status | aws enable | aws disable | mqtt status | mqtt enable | mqtt disable | cache status | cache enable | cache disable | cache purge | abrp status | abrp send | abrp enable | abrp disable | abrp clear");
}

void loop()
{
    c6DualCanLoop();
    c6JourneyEnergyLoop();
    c6ChargeJournalLoop();
    c6GpsLoop();
    c6TelemetryCadenceUpdate();
    c6NetworkLoop();
    c6AwsPrepareNetwork();
    c6LocalMqttPrepareNetwork();
    c6WebLoop();
    c6DisplayBleLoop();
    uint32_t serviceStartedMs = 0;
    uint32_t serviceElapsedMs = 0;
#ifdef MOT_DISPLAY_BLE
    c6CloudWorkerLoop();
#else
    serviceStartedMs = millis();
    c6AwsLoop();
    serviceElapsedMs = millis() - serviceStartedMs;
    if (serviceElapsedMs >= 250U)
        Serial.printf("Loop stall: service=aws durationMs=%lu\n",
                      static_cast<unsigned long>(serviceElapsedMs));
    c6DisplayBleLoop();
    serviceStartedMs = millis();
    c6LocalMqttLoop();
    serviceElapsedMs = millis() - serviceStartedMs;
    if (serviceElapsedMs >= 250U)
        Serial.printf("Loop stall: service=local-mqtt durationMs=%lu\n",
                      static_cast<unsigned long>(serviceElapsedMs));
#endif
    c6DisplayBleLoop();
    serviceStartedMs = millis();
    c6AbrpLoop();
    c6DataVolumeLoop();
    serviceElapsedMs = millis() - serviceStartedMs;
    if (serviceElapsedMs >= 250U)
        Serial.printf("Loop stall: service=abrp durationMs=%lu\n",
                      static_cast<unsigned long>(serviceElapsedMs));
    c6DisplayBleLoop();
    c6Bm2DiagLoop();
    c6StatusLedLoop();
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
        const bool gpsMoving = telemetry.display.valid &&
            millis() - telemetry.display.lastUpdateMs <= 5000 &&
            telemetry.display.speedKmh > 1.0f;
        Serial.printf("GPS altitude: %s\n", c6GpsStatusJson(gpsMoving).c_str());
        Serial.printf("WiFi=%s | AWS=%s | Local MQTT=%s\n", c6NetworkStatus().c_str(), c6AwsStatus().c_str(), c6LocalMqttStatusJson().c_str());
#ifdef MOT_DISPLAY_BLE
        Serial.println("Cloud worker=" + c6CloudWorkerStatusJson());
#endif
        Serial.println("Display BLE=" + c6DisplayBleStatusJson());

        const uint32_t packAgeMs = millis() - telemetry.bms.packStatusLastUpdateMs;
        const uint32_t cellsAgeMs = millis() - telemetry.bms.cellVoltagesLastUpdateMs;
        if (telemetry.bms.packStatusValid && packAgeMs <= 10000) {
            Serial.printf("Standard-CAN BMS: pack=%.3f V SOC-field=%.2f status=0x%02X plugged=%s age=%lu ms\n",
                          telemetry.bms.packVoltageMv / 1000.0,
                          telemetry.bms.socPercent,
                          static_cast<unsigned>(telemetry.bms.statusByte),
                          telemetry.bms.plugged ? "yes" : "no",
                          static_cast<unsigned long>(packAgeMs));
        }
        if (telemetry.bms.pioneerSocValid) {
            Serial.printf("Pioneer SOC: internal=%.1f display=%.1f age=%lu ms\n",
                          telemetry.bms.socInternal,
                          telemetry.bms.socDisplay,
                          static_cast<unsigned long>(millis() - telemetry.bms.pioneerSocLastUpdateMs));
        }
        if (telemetry.vehicleSoftware.vcuValid || telemetry.vehicleSoftware.ipcValid ||
            telemetry.vehicleSoftware.bduValid) {
            Serial.printf("Vehicle software: VCU=%u.%u.%u valid=%s IPC=%u.%u.%u valid=%s BDU=%u.%u.%u valid=%s\n",
                          telemetry.vehicleSoftware.vcuMajor,
                          telemetry.vehicleSoftware.vcuMinor,
                          telemetry.vehicleSoftware.vcuBuild,
                          telemetry.vehicleSoftware.vcuValid ? "yes" : "no",
                          telemetry.vehicleSoftware.ipcMajor,
                          telemetry.vehicleSoftware.ipcMinor,
                          telemetry.vehicleSoftware.ipcPatch,
                          telemetry.vehicleSoftware.ipcValid ? "yes" : "no",
                          telemetry.vehicleSoftware.bduMajor,
                          telemetry.vehicleSoftware.bduMinor,
                          telemetry.vehicleSoftware.bduPatch,
                          telemetry.vehicleSoftware.bduValid ? "yes" : "no");
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
