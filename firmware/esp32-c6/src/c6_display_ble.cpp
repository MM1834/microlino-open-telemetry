#include "c6_display_ble.h"

#ifdef MOT_DISPLAY_BLE

#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLESecurity.h>
#include <cmath>

#include "display/mot_display_ble_protocol.h"
#include "c6_aws.h"
#include "c6_charge_journal.h"
#include "c6_config.h"
#include "c6_journey_energy.h"
#include "c6_network.h"
#include "system/device_id.h"
#include "telemetry/telemetry.h"

namespace {
constexpr uint32_t kPublishIntervalMs = 200;
constexpr uint32_t kPowerFreshMs = 2500;
constexpr uint32_t kDisplayFreshMs = 5000;
constexpr float kFlowThresholdW = 200.0F;

BLECharacteristic *telemetryCharacteristic = nullptr;
BLECharacteristic *statusCharacteristic = nullptr;
BLECharacteristic *capabilitiesCharacteristic = nullptr;
BLECharacteristic *extendedTelemetryCharacteristic = nullptr;
BLECharacteristic *drivingStatusCharacteristic = nullptr;
bool clientConnected = false;
uint16_t sequence = 0;
uint32_t lastPublishMs = 0;
uint32_t notifications = 0;
uint32_t statusNotifications = 0;
uint32_t lastStatusPublishMs = 0;
uint16_t statusSequence = 0;
uint16_t extendedSequence = 0;
uint16_t drivingSequence = 0;

class ServerCallbacks final : public BLEServerCallbacks {
    void onConnect(BLEServer *) override
    {
        clientConnected = true;
        Serial.println("Display BLE: client connected");
    }

    void onDisconnect(BLEServer *) override
    {
        clientConnected = false;
        BLEDevice::startAdvertising();
        Serial.println("Display BLE: client disconnected; advertising resumed");
    }
};

uint16_t boundedAge(uint32_t timestampMs, uint32_t nowMs)
{
    const uint32_t age = nowMs - timestampMs;
    return static_cast<uint16_t>(min<uint32_t>(age, UINT16_MAX));
}

MotDisplayBle::TelemetryV1 makePacket(uint32_t nowMs)
{
    MotDisplayBle::TelemetryV1 packet{};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.sequence = ++sequence;
    packet.targetSocPercent = 0;
    packet.rangeToTargetKm = -1;
    packet.sourceAgeMs = UINT16_MAX;

    const bool powerFresh = telemetry.bms.packCurrentValid &&
        nowMs - telemetry.bms.packCurrentLastUpdateMs <= kPowerFreshMs &&
        isfinite(telemetry.bms.vehiclePowerW);
    if (powerFresh) {
        const float boundedDeciKw = constrain(telemetry.bms.vehiclePowerW / 100.0F,
                                              -32768.0F, 32767.0F);
        packet.powerDeciKw = static_cast<int16_t>(lroundf(boundedDeciKw));
        packet.sourceAgeMs = boundedAge(telemetry.bms.packCurrentLastUpdateMs, nowMs);
        packet.flags |= MotDisplayBle::kDataValid;
        if (telemetryIsCharging()) packet.flags |= MotDisplayBle::kCharging;
        else if (telemetry.bms.vehiclePowerW < -kFlowThresholdW) packet.flags |= MotDisplayBle::kRegenerating;
        else if (telemetry.bms.vehiclePowerW > kFlowThresholdW) packet.flags |= MotDisplayBle::kConsuming;
    }

    const bool displayFresh = telemetry.display.valid &&
        nowMs - telemetry.display.lastUpdateMs <= kDisplayFreshMs;
    if (displayFresh && isfinite(telemetry.display.soc) &&
        telemetry.display.soc >= 0.0F && telemetry.display.soc <= 100.0F) {
        packet.socPercent = static_cast<uint8_t>(lroundf(telemetry.display.soc));
        packet.flags |= MotDisplayBle::kSocValid;
        if (telemetry.display.estimatedRangeKm >= 0 && telemetry.display.estimatedRangeKm <= INT16_MAX) {
            packet.rangeToTargetKm = static_cast<int16_t>(telemetry.display.estimatedRangeKm);
            packet.flags |= MotDisplayBle::kRangeValid;
        }
    } else if (telemetry.bms.packStatusValid &&
               nowMs - telemetry.bms.packStatusLastUpdateMs <= kDisplayFreshMs &&
               isfinite(telemetry.bms.socPercent) &&
               telemetry.bms.socPercent >= 0.0F && telemetry.bms.socPercent <= 100.0F) {
        packet.socPercent = static_cast<uint8_t>(lroundf(telemetry.bms.socPercent));
        packet.flags |= MotDisplayBle::kSocValid;
    }

    MotDisplayBle::seal(packet);
    return packet;
}

MotDisplayBle::ExtendedTelemetryV1 makeExtendedPacket(uint32_t nowMs)
{
    MotDisplayBle::ExtendedTelemetryV1 packet{};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.sequence = ++extendedSequence;
    const bool packFresh = telemetry.bms.packStatusValid &&
        nowMs - telemetry.bms.packStatusLastUpdateMs <= kDisplayFreshMs;
    const bool currentFresh = telemetry.bms.packCurrentValid &&
        nowMs - telemetry.bms.packCurrentLastUpdateMs <= kPowerFreshMs;
    if (packFresh && currentFresh && isfinite(telemetry.bms.packCurrentA)) {
        packet.flags |= MotDisplayBle::kBatteryValuesValid;
        packet.packVoltageDeciV = static_cast<uint16_t>(constrain(
            lroundf(telemetry.bms.packVoltageMv / 100.0f), 0L, 65535L));
        packet.packCurrentDeciA = static_cast<int16_t>(constrain(
            lroundf(telemetry.bms.packCurrentA * 10.0f), -32768L, 32767L));
    }
    if (telemetry.bms.sohValid && packFresh && isfinite(telemetry.bms.sohPercent)) {
        packet.flags |= MotDisplayBle::kSohValid;
        packet.sohHundredths = static_cast<uint16_t>(constrain(
            lroundf(telemetry.bms.sohPercent * 100.0f), 0L, 10000L));
    }
    if (telemetry.bms.cellVoltagesValid &&
        nowMs - telemetry.bms.cellVoltagesLastUpdateMs <= kDisplayFreshMs) {
        packet.flags |= MotDisplayBle::kCellDeltaValid;
        packet.cellDeltaMv = telemetry.bms.cellVoltageDeltaMv;
    }
    const C6JourneyDisplaySnapshot journey = c6JourneyDisplaySnapshot();
    if (journey.active && journey.distanceValid) {
        packet.flags |= MotDisplayBle::kJourneyValid;
        packet.journeyDistanceDeciKm = static_cast<uint16_t>(constrain(
            lroundf(journey.distanceKm * 10.0f), 0L, 65535L));
        packet.journeyDurationMinutes = static_cast<uint16_t>(min<uint32_t>(
            journey.durationSeconds / 60U, UINT16_MAX));
        if (journey.consumptionValid) {
            packet.flags |= MotDisplayBle::kJourneyConsumptionValid;
            packet.journeyConsumptionDeciKwhPer100Km = static_cast<int16_t>(constrain(
                lroundf(journey.netKwhPer100Km * 10.0f), -32768L, 32767L));
        }
        if (journey.regenPeakW > 0) {
            packet.flags |= MotDisplayBle::kRegenPeakValid;
            packet.regenPeakDeciKw = static_cast<uint16_t>(journey.regenPeakW / 100U);
        }
    }
    const C6ChargeDisplaySnapshot charge = c6ChargeDisplaySnapshot();
    if (telemetryIsCharging() && currentFresh && isfinite(telemetry.bms.vehiclePowerW)) {
        packet.flags |= MotDisplayBle::kChargeValid;
        packet.chargePowerDeciKw = static_cast<uint16_t>(constrain(
            lroundf(fabsf(telemetry.bms.vehiclePowerW) / 100.0f), 0L, 65535L));
    }
    if (charge.active && charge.energyValid) {
        packet.flags |= MotDisplayBle::kChargeEnergyValid;
        packet.chargeEnergyWh = static_cast<uint16_t>(min<uint32_t>(charge.energyChargedWh, UINT16_MAX));
    }
    MotDisplayBle::seal(packet);
    return packet;
}

MotDisplayBle::DrivingStatusV1 makeDrivingPacket(uint32_t nowMs)
{
    MotDisplayBle::DrivingStatusV1 packet{};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.sequence = ++drivingSequence;
    packet.sourceAgeMs = UINT16_MAX;
    const bool displayFresh = telemetry.display.valid &&
        nowMs - telemetry.display.lastUpdateMs <= kDisplayFreshMs;
    if (displayFresh && isfinite(telemetry.display.speedKmh) &&
        telemetry.display.speedKmh >= 0.0F && telemetry.display.speedKmh <= 250.0F) {
        packet.flags |= MotDisplayBle::kDrivingSpeedValid;
        packet.speedDeciKmh = static_cast<uint16_t>(lroundf(telemetry.display.speedKmh * 10.0F));
        packet.sourceAgeMs = boundedAge(telemetry.display.lastUpdateMs, nowMs);
    }
    const bool indicatorsFresh = telemetry.display.indicatorStateValid &&
        nowMs - telemetry.display.indicatorLastUpdateMs <= kDisplayFreshMs;
    if (indicatorsFresh) {
        packet.flags |= MotDisplayBle::kIndicatorStateValid;
        if (telemetry.display.leftIndicatorActive)
            packet.flags |= MotDisplayBle::kLeftIndicatorActive;
        if (telemetry.display.rightIndicatorActive)
            packet.flags |= MotDisplayBle::kRightIndicatorActive;
        packet.sourceAgeMs = min<uint16_t>(
            packet.sourceAgeMs, boundedAge(telemetry.display.indicatorLastUpdateMs, nowMs));
    }
    const bool safetyFresh = telemetry.display.safetyStateValid &&
        nowMs - telemetry.display.safetyLastUpdateMs <= kDisplayFreshMs;
    if (safetyFresh) {
        packet.flags |= MotDisplayBle::kSafetyStateValid;
        if (telemetry.display.parkingBrakeActive)
            packet.flags |= MotDisplayBle::kParkingBrakeActive;
        if (telemetry.display.warning90Code != 0U) {
            packet.flags |= MotDisplayBle::kWarning90Active;
            packet.warning90Code = telemetry.display.warning90Code;
        }
        if (telemetry.display.warningHblAhActive)
            packet.flags |= MotDisplayBle::kWarningHblAhActive;
        if (telemetry.display.drivetrainWarningYellow)
            packet.flags |= MotDisplayBle::kDrivetrainWarningYellow;
        packet.sourceAgeMs = min<uint16_t>(
            packet.sourceAgeMs, boundedAge(telemetry.display.safetyLastUpdateMs, nowMs));
    }
    const bool candidatesFresh = telemetry.display.candidateStateLastUpdateMs != 0U &&
        nowMs - telemetry.display.candidateStateLastUpdateMs <= kDisplayFreshMs;
    if (candidatesFresh) {
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
        packet.sourceAgeMs = min<uint16_t>(
            packet.sourceAgeMs,
            boundedAge(telemetry.display.candidateStateLastUpdateMs, nowMs));
    }
    MotDisplayBle::seal(packet);
    return packet;
}

void copyBounded(char *destination, size_t capacity, const String &source)
{
    if (!destination || !capacity) return;
    strlcpy(destination, source.c_str(), capacity);
}

MotDisplayBle::StatusV1 makeStatusPacket()
{
    MotDisplayBle::StatusV1 packet{};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.sequence = ++statusSequence;
    packet.wifiRssi = static_cast<int8_t>(constrain(c6NetworkRssi(), -127, 0));
    if (c6NetworkHomeConfigured()) packet.flags |= MotDisplayBle::kWifi1Configured;
    if (c6NetworkMobileConfigured()) packet.flags |= MotDisplayBle::kWifi2Configured;
    if (c6NetworkOnline()) packet.flags |= MotDisplayBle::kNetworkOnline;
    const String activeProfile = c6NetworkProfileName();
    if (activeProfile == "home") packet.flags |= MotDisplayBle::kWifi1Active;
    if (activeProfile == "mobile") packet.flags |= MotDisplayBle::kWifi2Active;
    if (c6Config.motCloudEnabled) packet.flags |= MotDisplayBle::kAwsEnabled;
    if (c6AwsConnected()) packet.flags |= MotDisplayBle::kAwsConnected;
    if (c6NetworkApActive()) packet.flags2 |= MotDisplayBle::kAccessPointActive;
    if (c6AwsRecentlyHealthy()) packet.flags2 |= MotDisplayBle::kAwsRecentlyHealthy;
    if (c6AwsStatus().indexOf("credentials=yes") >= 0) {
        packet.flags |= MotDisplayBle::kAwsCredentialsReady;
    }
    copyBounded(packet.wifi1Name, sizeof(packet.wifi1Name), c6Config.wifiSsid);
    copyBounded(packet.wifi2Name, sizeof(packet.wifi2Name), c6Config.wifi2Ssid);
    const String activeIp = c6NetworkOnline() ? c6NetworkIp() :
        c6NetworkApActive() ? c6NetworkApIp() : String();
    copyBounded(packet.ipAddress, sizeof(packet.ipAddress), activeIp);
    MotDisplayBle::seal(packet);
    return packet;
}
}  // namespace

void c6DisplayBleSetup()
{
    String name = "MOT-" + motDeviceId();
    if (name.length() > 24) name = name.substring(0, 24);
    BLEDevice::init(name.c_str());
    BLESecurity *security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setAuthenticationMode(true, false, true);
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    BLEService *service = server->createService(MotDisplayBle::kServiceUuid);
    telemetryCharacteristic = service->createCharacteristic(
        MotDisplayBle::kTelemetryUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    statusCharacteristic = service->createCharacteristic(
        MotDisplayBle::kStatusUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    capabilitiesCharacteristic = service->createCharacteristic(
        MotDisplayBle::kCapabilitiesUuid, BLECharacteristic::PROPERTY_READ);
    extendedTelemetryCharacteristic = service->createCharacteristic(
        MotDisplayBle::kExtendedTelemetryUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    drivingStatusCharacteristic = service->createCharacteristic(
        MotDisplayBle::kDrivingStatusUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    telemetryCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    statusCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    capabilitiesCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    extendedTelemetryCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    drivingStatusCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    MotDisplayBle::TelemetryV1 initial = makePacket(millis());
    telemetryCharacteristic->setValue(reinterpret_cast<uint8_t *>(&initial), sizeof(initial));
    MotDisplayBle::StatusV1 initialStatus = makeStatusPacket();
    statusCharacteristic->setValue(reinterpret_cast<uint8_t *>(&initialStatus), sizeof(initialStatus));
    MotDisplayBle::CapabilitiesV1 capabilities{};
    capabilities.magic = MotDisplayBle::kMagic;
    capabilities.version = MotDisplayBle::kCapabilitiesVersion;
    capabilities.protocolMin = MotDisplayBle::kProtocolVersion;
    capabilities.protocolMax = MotDisplayBle::kProtocolVersion;
    capabilities.screenFlags = MotDisplayBle::kPowerScreen | MotDisplayBle::kAdapterStatusScreen;
    capabilities.featureFlags = MotDisplayBle::kTelemetryV1Feature |
        MotDisplayBle::kStatusV1Feature | MotDisplayBle::kBondedEncryptionFeature |
        MotDisplayBle::kExtendedTelemetryV1Feature | MotDisplayBle::kDrivingStatusV1Feature;
    MotDisplayBle::seal(capabilities);
    capabilitiesCharacteristic->setValue(reinterpret_cast<uint8_t *>(&capabilities),
                                         sizeof(capabilities));
    MotDisplayBle::ExtendedTelemetryV1 initialExtended = makeExtendedPacket(millis());
    extendedTelemetryCharacteristic->setValue(
        reinterpret_cast<uint8_t *>(&initialExtended), sizeof(initialExtended));
    MotDisplayBle::DrivingStatusV1 initialDriving = makeDrivingPacket(millis());
    drivingStatusCharacteristic->setValue(
        reinterpret_cast<uint8_t *>(&initialDriving), sizeof(initialDriving));
    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(MotDisplayBle::kServiceUuid);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.printf("Display BLE: advertising as %s, protocol v%u\n",
                  name.c_str(), MotDisplayBle::kProtocolVersion);
}

void c6DisplayBleLoop()
{
    if (!telemetryCharacteristic) return;
    const uint32_t nowMs = millis();
    if (nowMs - lastPublishMs < kPublishIntervalMs) return;
    lastPublishMs = nowMs;

    MotDisplayBle::TelemetryV1 packet = makePacket(nowMs);
    telemetryCharacteristic->setValue(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
    if (clientConnected) {
        telemetryCharacteristic->notify();
        ++notifications;
        MotDisplayBle::ExtendedTelemetryV1 extended = makeExtendedPacket(nowMs);
        extendedTelemetryCharacteristic->setValue(reinterpret_cast<uint8_t *>(&extended),
                                                  sizeof(extended));
        extendedTelemetryCharacteristic->notify();
        MotDisplayBle::DrivingStatusV1 driving = makeDrivingPacket(nowMs);
        drivingStatusCharacteristic->setValue(reinterpret_cast<uint8_t *>(&driving),
                                               sizeof(driving));
        drivingStatusCharacteristic->notify();
    }
    if (nowMs - lastStatusPublishMs >= 1000U) {
        lastStatusPublishMs = nowMs;
        MotDisplayBle::StatusV1 status = makeStatusPacket();
        statusCharacteristic->setValue(reinterpret_cast<uint8_t *>(&status), sizeof(status));
        if (clientConnected) {
            statusCharacteristic->notify();
            ++statusNotifications;
        }
    }
}

String c6DisplayBleStatusJson()
{
    return String("{\"enabled\":true,\"connected\":") +
        (clientConnected ? "true" : "false") +
        ",\"sequence\":" + String(sequence) +
        ",\"notifications\":" + String(notifications) +
        ",\"statusNotifications\":" + String(statusNotifications) + "}";
}

#else

void c6DisplayBleSetup() {}
void c6DisplayBleLoop() {}
String c6DisplayBleStatusJson() { return "{\"enabled\":false}"; }

#endif
