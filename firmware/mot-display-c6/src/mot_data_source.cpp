#include "mot_data_source.h"

#include <cmath>
#include <driver/twai.h>

#include "decoders/decoder_display_can.h"
#include "mot_ble_client.h"
#include "telemetry/telemetry.h"

namespace {
// GPIO12/GPIO13 are the ESP32-C6 native USB D-/D+ signals on this board and
// must remain untouched. GPIO1/GPIO2/GPIO3 are exposed on the expansion header.
constexpr gpio_num_t kModeDetectPin = GPIO_NUM_2;
constexpr gpio_num_t kCanRxPin = GPIO_NUM_1;
// TWAI requires a TX GPIO even in listen-only mode. GPIO3 is assigned internally
// but must remain physically disconnected from transceiver TXD.
constexpr gpio_num_t kUnusedCanTxPin = GPIO_NUM_3;
constexpr uint32_t kDisplayFreshMs = 2500U;
constexpr int16_t kLocalChargeMinPowerDeciKw = 3;

bool standalone = false;
bool canReady = false;
uint32_t canFrames = 0;
uint16_t sequence = 0;
uint16_t drivingSequence = 0;
bool localChargeActive = false;

uint16_t boundedAge(uint32_t timestampMs)
{
    return static_cast<uint16_t>(min<uint32_t>(millis() - timestampMs, UINT16_MAX));
}

bool fresh(uint32_t timestampMs)
{
    return timestampMs != 0U && millis() - timestampMs <= kDisplayFreshMs;
}

bool localChargeEvidence()
{
    if (!fresh(telemetry.charging.lastUpdateMs) ||
        telemetry.charging.powerSigned < kLocalChargeMinPowerDeciKw) {
        return false;
    }
    if (telemetry.charging.isCharging) return true;
    return fresh(telemetry.display.lastUpdateMs) &&
        isfinite(telemetry.display.speedKmh) && telemetry.display.speedKmh <= 0.5F &&
        telemetry.display.gearStateValid &&
        telemetry.display.gearState == MotDisplayBle::kGearNeutral;
}

void updateLocalChargeState()
{
    localChargeActive = localChargeEvidence();
}

void setupStandaloneCan()
{
    telemetryInit();
    const twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        kUnusedCanTxPin, kCanRxPin, TWAI_MODE_LISTEN_ONLY);
    const twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
        Serial.println("Standalone CAN: TWAI install failed");
        return;
    }
    if (twai_start() != ESP_OK) {
        Serial.println("Standalone CAN: TWAI start failed");
        twai_driver_uninstall();
        return;
    }
    canReady = true;
    Serial.printf("Standalone CAN: listen-only RX=%d, unused TX=%d\n",
                  static_cast<int>(kCanRxPin), static_cast<int>(kUnusedCanTxPin));
}

void receiveStandaloneCan()
{
    if (!canReady) return;
    twai_message_t message{};
    uint8_t handled = 0;
    while (handled < 24U && twai_receive(&message, 0) == ESP_OK) {
        MotCanFrame frame{};
        frame.id = message.identifier;
        frame.extended = message.extd;
        frame.dlc = min<uint8_t>(message.data_length_code, 8U);
        memcpy(frame.data, message.data, frame.dlc);
        decoderDisplayCanHandleFrame(frame);
        ++canFrames;
        ++handled;
    }
}
}  // namespace

void motDataSourceSetup()
{
    pinMode(static_cast<uint8_t>(kModeDetectPin), INPUT_PULLUP);
    delay(5);
    standalone = digitalRead(static_cast<uint8_t>(kModeDetectPin)) == LOW;
    if (standalone) {
        Serial.println("Data source locked for this boot: STANDALONE DISPLAY-CAN");
        setupStandaloneCan();
    } else {
        Serial.println("Data source locked for this boot: MOT BLE");
        motBleClientSetup();
    }
}

void motDataSourceLoop()
{
    if (standalone) {
        receiveStandaloneCan();
        updateLocalChargeState();
    }
    else motBleClientLoop();
}

bool motDataSourceStandalone() { return standalone; }
uint32_t motDataSourceCanFrames() { return canFrames; }

bool motDataSourceLatest(MotDisplayBle::TelemetryV1 &packet, uint32_t &ageMs)
{
    if (!standalone) return motBleClientLatest(packet, ageMs);
    packet = {};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.sequence = ++sequence;
    packet.rangeToTargetKm = -1;
    packet.sourceAgeMs = UINT16_MAX;
    if (fresh(telemetry.display.lastUpdateMs)) {
        ageMs = millis() - telemetry.display.lastUpdateMs;
        packet.sourceAgeMs = boundedAge(telemetry.display.lastUpdateMs);
        if (isfinite(telemetry.display.soc) && telemetry.display.soc >= 0.0F &&
            telemetry.display.soc <= 100.0F) {
            packet.socPercent = static_cast<uint8_t>(lroundf(telemetry.display.soc));
            packet.flags |= MotDisplayBle::kSocValid;
            if (telemetry.display.estimatedRangeKm >= 0 &&
                telemetry.display.estimatedRangeKm <= INT16_MAX) {
                packet.rangeToTargetKm = static_cast<int16_t>(telemetry.display.estimatedRangeKm);
                packet.flags |= MotDisplayBle::kRangeValid;
            }
        }
    } else ageMs = UINT32_MAX;
    if (fresh(telemetry.charging.lastUpdateMs)) {
        packet.powerDeciKw = static_cast<int16_t>(constrain(
            telemetry.charging.powerSigned, -32768, 32767));
        packet.flags |= MotDisplayBle::kDataValid;
        if (localChargeActive) packet.flags |= MotDisplayBle::kCharging;
        else if (packet.powerDeciKw < -2) packet.flags |= MotDisplayBle::kRegenerating;
        else if (packet.powerDeciKw > 2) packet.flags |= MotDisplayBle::kConsuming;
        packet.sourceAgeMs = min<uint16_t>(packet.sourceAgeMs,
            boundedAge(telemetry.charging.lastUpdateMs));
    }
    MotDisplayBle::seal(packet);
    return (packet.flags & (MotDisplayBle::kDataValid | MotDisplayBle::kSocValid)) != 0U;
}

bool motDataSourceLatestDriving(MotDisplayBle::DrivingStatusV1 &packet, uint32_t &ageMs)
{
    if (!standalone) return motBleClientLatestDriving(packet, ageMs);
    packet = {};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.sequence = ++drivingSequence;
    packet.sourceAgeMs = UINT16_MAX;
    if (fresh(telemetry.display.lastUpdateMs) && isfinite(telemetry.display.speedKmh)) {
        packet.flags |= MotDisplayBle::kDrivingSpeedValid;
        packet.speedDeciKmh = static_cast<uint16_t>(constrain(
            lroundf(telemetry.display.speedKmh * 10.0F), 0L, 2500L));
        packet.sourceAgeMs = boundedAge(telemetry.display.lastUpdateMs);
    }
    if (telemetry.display.indicatorStateValid && fresh(telemetry.display.indicatorLastUpdateMs)) {
        packet.flags |= MotDisplayBle::kIndicatorStateValid;
        if (telemetry.display.leftIndicatorActive) packet.flags |= MotDisplayBle::kLeftIndicatorActive;
        if (telemetry.display.rightIndicatorActive) packet.flags |= MotDisplayBle::kRightIndicatorActive;
        packet.sourceAgeMs = min<uint16_t>(packet.sourceAgeMs,
            boundedAge(telemetry.display.indicatorLastUpdateMs));
    }
    if (telemetry.display.safetyStateValid && fresh(telemetry.display.safetyLastUpdateMs)) {
        packet.flags |= MotDisplayBle::kSafetyStateValid;
        if (telemetry.display.parkingBrakeActive) packet.flags |= MotDisplayBle::kParkingBrakeActive;
        if (telemetry.display.warning90Code != 0U) packet.flags |= MotDisplayBle::kWarning90Active;
        if (telemetry.display.warningHblAhActive) packet.flags |= MotDisplayBle::kWarningHblAhActive;
        if (telemetry.display.drivetrainWarningYellow) packet.flags |= MotDisplayBle::kDrivetrainWarningYellow;
        packet.warning90Code = telemetry.display.warning90Code;
        packet.sourceAgeMs = min<uint16_t>(packet.sourceAgeMs,
            boundedAge(telemetry.display.safetyLastUpdateMs));
    }
    if (fresh(telemetry.display.candidateStateLastUpdateMs)) {
        packet.flags |= MotDisplayBle::kCandidateStatesValid;
        if (telemetry.display.gearStateValid && telemetry.display.gearState <= 2U) {
            const uint16_t gear = static_cast<uint16_t>(telemetry.display.gearState);
            packet.flags |= gear << MotDisplayBle::kGearStateShift;
        } else {
            packet.flags |= static_cast<uint16_t>(MotDisplayBle::kGearUnknown)
                << MotDisplayBle::kGearStateShift;
        }
        if (telemetry.display.lightStateValid) {
            const uint16_t light =
                (telemetry.display.positionLightActive ? 1U : 0U) |
                (telemetry.display.lowBeamActive ? 2U : 0U);
            packet.flags |= light << MotDisplayBle::kLightStateShift;
        }
        if (telemetry.display.driveModeStateValid) {
            const uint16_t mode =
                (telemetry.display.ecoModeActive ? 1U : 0U) |
                (telemetry.display.sportModeActive ? 2U : 0U);
            packet.flags |= mode << MotDisplayBle::kDriveModeShift;
        }
        packet.sourceAgeMs = min<uint16_t>(packet.sourceAgeMs,
            boundedAge(telemetry.display.candidateStateLastUpdateMs));
    }
    ageMs = packet.sourceAgeMs;
    MotDisplayBle::seal(packet);
    return packet.flags != 0U;
}

bool motDataSourceLatestStatus(MotDisplayBle::StatusV1 &packet, uint32_t &ageMs)
{
    if (!standalone) return motBleClientLatestStatus(packet, ageMs);
    packet = {};
    ageMs = UINT32_MAX;
    return false;
}

bool motDataSourceLatestVehicleBoardNetwork(MotDisplayBle::VehicleBoardNetworkV1 &packet,
                                            uint32_t &ageMs)
{
    if (!standalone) return motBleClientLatestVehicleBoardNetwork(packet, ageMs);
    packet = {};
    ageMs = UINT32_MAX;
    return false;
}

bool motDataSourceLatestSoftwareVersions(MotDisplayBle::SoftwareVersionsV1 &packet,
                                        uint32_t &ageMs)
{
    if (!standalone) return motBleClientLatestSoftwareVersions(packet, ageMs);
    packet = MotDisplayBle::SoftwareVersionsV1{};
    ageMs = UINT32_MAX;
    return false;
}

bool motDataSourceLatestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet, uint32_t &ageMs)
{
    if (!standalone) return motBleClientLatestExtended(packet, ageMs);
    packet = {};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    if (fresh(telemetry.display.lastUpdateMs) && isfinite(telemetry.display.tripKm)) {
        packet.flags |= MotDisplayBle::kJourneyValid;
        packet.journeyDistanceDeciKm = static_cast<uint16_t>(constrain(
            lroundf(telemetry.display.tripKm * 10.0F), 0L, 65535L));
        ageMs = millis() - telemetry.display.lastUpdateMs;
    } else ageMs = UINT32_MAX;
    MotDisplayBle::seal(packet);
    return packet.flags != 0U;
}

const char *motDataSourceStatus()
{
    if (!standalone) return motBleClientStatus();
    if (!canReady) return "CAN FEHLER";
    return canFrames ? "DISPLAY-CAN" : "WARTE AUF CAN";
}

void motDataSourceForgetPairing()
{
    if (!standalone) motBleClientForgetPairing();
}
