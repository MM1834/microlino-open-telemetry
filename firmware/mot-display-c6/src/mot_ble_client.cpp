#include "mot_ble_client.h"

#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <BLESecurity.h>
#include <Preferences.h>
#include <cstring>

#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_store.h"
#endif

namespace {
constexpr uint32_t kScanRetryMs = 5000;
constexpr uint32_t kPacketStaleMs = 2500;
constexpr uint32_t kScanDurationSeconds = 3;
#ifndef MOT_DISPLAY_INITIAL_PAIRING_MS
#define MOT_DISPLAY_INITIAL_PAIRING_MS 120000
#endif
constexpr uint32_t kInitialPairingWindowMs = MOT_DISPLAY_INITIAL_PAIRING_MS;
constexpr char kPreferencesNamespace[] = "mot-display";
constexpr char kPeerAddressKey[] = "peer";

BLEAdvertisedDevice *candidate = nullptr;
BLEClient *client = nullptr;
BLERemoteCharacteristic *telemetryCharacteristic = nullptr;
BLERemoteCharacteristic *statusCharacteristic = nullptr;
BLERemoteCharacteristic *extendedTelemetryCharacteristic = nullptr;
BLERemoteCharacteristic *drivingStatusCharacteristic = nullptr;
BLERemoteCharacteristic *presentationConfigCharacteristic = nullptr;
BLERemoteCharacteristic *chargeSummaryCharacteristic = nullptr;
BLERemoteCharacteristic *journeyAltitudeCharacteristic = nullptr;
BLERemoteCharacteristic *vehicleBoardNetworkCharacteristic = nullptr;
BLERemoteCharacteristic *softwareVersionsCharacteristic = nullptr;
volatile bool connected = false;
volatile bool connectPending = false;
volatile bool connectWorkerRunning = false;
volatile bool scanRunning = false;
bool pairingMode = false;
bool securityRecoveryAttempted = false;
uint32_t pairingDeadlineMs = 0;
String boundPeerAddress;
Preferences preferences;
uint32_t lastScanMs = 0;
MotDisplayBle::TelemetryV1 latestPacket{};
uint32_t latestPacketMs = 0;
MotDisplayBle::StatusV1 latestStatus{};
uint32_t latestStatusMs = 0;
MotDisplayBle::ExtendedTelemetryV1 latestExtended{};
uint32_t latestExtendedMs = 0;
MotDisplayBle::DrivingStatusV1 latestDriving{};
uint32_t latestDrivingMs = 0;
MotDisplayBle::PresentationConfigV1 latestPresentationConfig{};
uint32_t latestPresentationConfigMs = 0;
MotDisplayBle::ChargeSummaryV1 latestChargeSummary{};
uint32_t latestChargeSummaryMs = 0;
MotDisplayBle::JourneyAltitudeV1 latestJourneyAltitude{};
uint32_t latestJourneyAltitudeMs = 0;
MotDisplayBle::VehicleBoardNetworkV1 latestVehicleBoardNetwork{};
uint32_t latestVehicleBoardNetworkMs = 0;
MotDisplayBle::SoftwareVersionsV1 latestSoftwareVersions{};
uint32_t latestSoftwareVersionsMs = 0;
portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
const char *statusText = "SUCHE MOT";

bool clearStoredBonds()
{
#if defined(CONFIG_NIMBLE_ENABLED)
    const int result = ble_store_clear();
    Serial.printf("BLE: local bond store clear rc=%d\n", result);
    return result == 0;
#else
    Serial.println("BLE: local bond-store reset is unavailable on this BLE stack");
    return false;
#endif
}

bool addressAllowed(BLEAdvertisedDevice &device)
{
    if (boundPeerAddress.length()) {
        return device.getAddress().toString().equalsIgnoreCase(boundPeerAddress);
    }
    return pairingMode && static_cast<int32_t>(pairingDeadlineMs - millis()) > 0;
}

void acceptPacket(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::TelemetryV1)) return;
    MotDisplayBle::TelemetryV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) {
        Serial.println("BLE: invalid display packet rejected");
        return;
    }
    portENTER_CRITICAL(&packetMux);
    latestPacket = packet;
    latestPacketMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void notificationCallback(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    acceptPacket(data, length);
}

void acceptStatus(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::StatusV1)) return;
    MotDisplayBle::StatusV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) {
        Serial.println("BLE: invalid status packet rejected");
        return;
    }
    portENTER_CRITICAL(&packetMux);
    latestStatus = packet;
    latestStatusMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void statusNotificationCallback(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    acceptStatus(data, length);
}

void acceptExtended(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::ExtendedTelemetryV1)) return;
    MotDisplayBle::ExtendedTelemetryV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) {
        Serial.println("BLE: invalid extended display packet rejected");
        return;
    }
    portENTER_CRITICAL(&packetMux);
    latestExtended = packet;
    latestExtendedMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void extendedNotificationCallback(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    acceptExtended(data, length);
}

void acceptChargeSummary(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::ChargeSummaryV1)) return;
    MotDisplayBle::ChargeSummaryV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) {
        Serial.println("BLE: invalid charge summary rejected");
        return;
    }
    portENTER_CRITICAL(&packetMux);
    latestChargeSummary = packet;
    latestChargeSummaryMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void chargeSummaryNotificationCallback(BLERemoteCharacteristic *, uint8_t *data,
                                       size_t length, bool)
{
    acceptChargeSummary(data, length);
}

void acceptJourneyAltitude(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::JourneyAltitudeV1)) return;
    MotDisplayBle::JourneyAltitudeV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) {
        Serial.println("BLE: invalid journey altitude rejected");
        return;
    }
    portENTER_CRITICAL(&packetMux);
    latestJourneyAltitude = packet;
    latestJourneyAltitudeMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void journeyAltitudeNotificationCallback(BLERemoteCharacteristic *, uint8_t *data,
                                         size_t length, bool)
{
    acceptJourneyAltitude(data, length);
}

void acceptVehicleBoardNetwork(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::VehicleBoardNetworkV1)) return;
    MotDisplayBle::VehicleBoardNetworkV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet) ||
        packet.historyCount > MotDisplayBle::kBoardVoltageHistorySamples) return;
    portENTER_CRITICAL(&packetMux);
    latestVehicleBoardNetwork = packet;
    latestVehicleBoardNetworkMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void vehicleBoardNetworkNotificationCallback(BLERemoteCharacteristic *, uint8_t *data,
                                             size_t length, bool)
{
    acceptVehicleBoardNetwork(data, length);
}

void acceptSoftwareVersions(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::SoftwareVersionsV1)) return;
    MotDisplayBle::SoftwareVersionsV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) return;
    portENTER_CRITICAL(&packetMux);
    latestSoftwareVersions = packet;
    latestSoftwareVersionsMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void softwareVersionsNotificationCallback(BLERemoteCharacteristic *, uint8_t *data,
                                          size_t length, bool)
{
    acceptSoftwareVersions(data, length);
}

void acceptDriving(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::DrivingStatusV1)) return;
    MotDisplayBle::DrivingStatusV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) return;
    portENTER_CRITICAL(&packetMux);
    latestDriving = packet;
    latestDrivingMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void drivingNotificationCallback(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    acceptDriving(data, length);
}

void acceptPresentationConfig(const uint8_t *data, size_t length)
{
    if (!data || length != sizeof(MotDisplayBle::PresentationConfigV1)) return;
    MotDisplayBle::PresentationConfigV1 packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!MotDisplayBle::valid(packet)) return;
    portENTER_CRITICAL(&packetMux);
    latestPresentationConfig = packet;
    latestPresentationConfigMs = millis();
    portEXIT_CRITICAL(&packetMux);
}

void presentationConfigNotificationCallback(BLERemoteCharacteristic *, uint8_t *data,
                                              size_t length, bool)
{
    acceptPresentationConfig(data, length);
}


class ClientCallbacks final : public BLEClientCallbacks {
    void onConnect(BLEClient *) override {}

    void onDisconnect(BLEClient *) override
    {
        connected = false;
        telemetryCharacteristic = nullptr;
        statusCharacteristic = nullptr;
        extendedTelemetryCharacteristic = nullptr;
        drivingStatusCharacteristic = nullptr;
        presentationConfigCharacteristic = nullptr;
        chargeSummaryCharacteristic = nullptr;
        journeyAltitudeCharacteristic = nullptr;
        vehicleBoardNetworkCharacteristic = nullptr;
        softwareVersionsCharacteristic = nullptr;
        statusText = "VERBINDUNG WEG";
        Serial.println("BLE: MOT disconnected");
    }
};

class ScanCallbacks final : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override
    {
#ifdef MOT_DISPLAY_SCAN_DIAGNOSTICS
        Serial.printf("BLE scan: %s RSSI %d service=%s\n",
                      advertisedDevice.getAddress().toString().c_str(),
                      advertisedDevice.getRSSI(),
                      advertisedDevice.haveServiceUUID()
                          ? advertisedDevice.getServiceUUID().toString().c_str() : "-");
#endif
        if (!advertisedDevice.haveServiceUUID() ||
            !advertisedDevice.isAdvertisingService(BLEUUID(MotDisplayBle::kServiceUuid))) return;
        if (!addressAllowed(advertisedDevice)) return;
        BLEDevice::getScan()->stop();
        delete candidate;
        candidate = new BLEAdvertisedDevice(advertisedDevice);
        connectPending = true;
        scanRunning = false;
        statusText = "VERBINDE";
        Serial.printf("BLE: MOT found at %s, RSSI %d dBm\n",
                      candidate->getAddress().toString().c_str(), candidate->getRSSI());
    }
};

void scanComplete(BLEScanResults)
{
    scanRunning = false;
    Serial.println("BLE: asynchronous scan complete");
}

void startScan()
{
    if (!boundPeerAddress.length() &&
        (!pairingMode || static_cast<int32_t>(pairingDeadlineMs - millis()) <= 0)) {
        pairingMode = false;
        statusText = "KOPPELN ABGELAUFEN";
        return;
    }
    BLEScan *scanner = BLEDevice::getScan();
    scanner->clearResults();
    scanRunning = true;
    lastScanMs = millis();
    statusText = boundPeerAddress.length() ? "SUCHE GEKOPPELTES MOT" : "KOPPELBEREIT";
    Serial.println("BLE: scanning for MOT display service");
    if (!scanner->start(kScanDurationSeconds, scanComplete, false)) {
        scanRunning = false;
        statusText = "BLE-SUCHE FEHLER";
        Serial.println("BLE: asynchronous scan failed to start");
    }
}

bool connectToCandidate(BLEAdvertisedDevice *target)
{
    if (!target) return false;
    if (!client) {
        client = BLEDevice::createClient();
        client->setClientCallbacks(new ClientCallbacks());
    }
    if (!client->connect(target)) {
        statusText = "MOT NICHT ERREICHBAR";
        return false;
    }
    if (!client->secureConnection()) {
        client->disconnect();
        if (!securityRecoveryAttempted) {
            securityRecoveryAttempted = true;
            clearStoredBonds();
            lastScanMs = millis() - kScanRetryMs;
            statusText = "KOPPLUNG WIRD ERNEUERT";
            Serial.println("BLE: security failed; cleared local bonds for one recovery attempt");
        } else {
            statusText = "KOPPLUNG FEHLER";
        }
        return false;
    }
    client->setMTU(128);
    BLERemoteService *service = client->getService(BLEUUID(MotDisplayBle::kServiceUuid));
    if (!service) {
        client->disconnect();
        statusText = "FALSCHER DIENST";
        return false;
    }
    telemetryCharacteristic = service->getCharacteristic(BLEUUID(MotDisplayBle::kTelemetryUuid));
    statusCharacteristic = service->getCharacteristic(BLEUUID(MotDisplayBle::kStatusUuid));
    BLERemoteCharacteristic *capabilitiesCharacteristic =
        service->getCharacteristic(BLEUUID(MotDisplayBle::kCapabilitiesUuid));
    if (!telemetryCharacteristic || !telemetryCharacteristic->canNotify() ||
        !statusCharacteristic || !statusCharacteristic->canNotify() ||
        !capabilitiesCharacteristic || !capabilitiesCharacteristic->canRead()) {
        client->disconnect();
        statusText = "FALSCHES PROTOKOLL";
        return false;
    }
    const String capabilityValue = capabilitiesCharacteristic->readValue();
    MotDisplayBle::CapabilitiesV1 capabilities{};
    if (capabilityValue.length() != sizeof(capabilities)) {
        client->disconnect();
        statusText = "VERSION NICHT KOMPATIBEL";
        return false;
    }
    memcpy(&capabilities, capabilityValue.c_str(), sizeof(capabilities));
    constexpr uint16_t kRequiredFeatures = MotDisplayBle::kTelemetryV1Feature |
        MotDisplayBle::kStatusV1Feature | MotDisplayBle::kBondedEncryptionFeature;
    if (!MotDisplayBle::valid(capabilities) ||
        (capabilities.featureFlags & kRequiredFeatures) != kRequiredFeatures) {
        client->disconnect();
        statusText = "VERSION NICHT KOMPATIBEL";
        return false;
    }
    if (capabilities.featureFlags & MotDisplayBle::kExtendedTelemetryV1Feature) {
        extendedTelemetryCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kExtendedTelemetryUuid));
        if (extendedTelemetryCharacteristic && extendedTelemetryCharacteristic->canRead()) {
            const String value = extendedTelemetryCharacteristic->readValue();
            acceptExtended(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
        }
        if (extendedTelemetryCharacteristic && extendedTelemetryCharacteristic->canNotify())
            extendedTelemetryCharacteristic->registerForNotify(extendedNotificationCallback);
        else extendedTelemetryCharacteristic = nullptr;
    }
    if (capabilities.featureFlags & MotDisplayBle::kDrivingStatusV1Feature) {
        drivingStatusCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kDrivingStatusUuid));
        if (drivingStatusCharacteristic && drivingStatusCharacteristic->canRead()) {
            const String value = drivingStatusCharacteristic->readValue();
            acceptDriving(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
        }
        if (drivingStatusCharacteristic && drivingStatusCharacteristic->canNotify())
            drivingStatusCharacteristic->registerForNotify(drivingNotificationCallback);
        else drivingStatusCharacteristic = nullptr;
    }
    if (capabilities.featureFlags & MotDisplayBle::kPresentationConfigV1Feature) {
        presentationConfigCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kPresentationConfigUuid));
        if (presentationConfigCharacteristic && presentationConfigCharacteristic->canRead()) {
            const String value = presentationConfigCharacteristic->readValue();
            acceptPresentationConfig(reinterpret_cast<const uint8_t *>(value.c_str()),
                                     value.length());
        }
        if (presentationConfigCharacteristic && presentationConfigCharacteristic->canNotify())
            presentationConfigCharacteristic->registerForNotify(
                presentationConfigNotificationCallback);
        if (!presentationConfigCharacteristic || !presentationConfigCharacteristic->canWrite())
            presentationConfigCharacteristic = nullptr;
    }
    if (capabilities.featureFlags & MotDisplayBle::kChargeSummaryV1Feature) {
        chargeSummaryCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kChargeSummaryUuid));
        if (chargeSummaryCharacteristic && chargeSummaryCharacteristic->canRead()) {
            const String value = chargeSummaryCharacteristic->readValue();
            acceptChargeSummary(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
        }
        if (chargeSummaryCharacteristic && chargeSummaryCharacteristic->canNotify())
            chargeSummaryCharacteristic->registerForNotify(chargeSummaryNotificationCallback);
        else chargeSummaryCharacteristic = nullptr;
    }
    if (capabilities.featureFlags & MotDisplayBle::kJourneyAltitudeV1Feature) {
        journeyAltitudeCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kJourneyAltitudeUuid));
        if (journeyAltitudeCharacteristic && journeyAltitudeCharacteristic->canRead()) {
            const String value = journeyAltitudeCharacteristic->readValue();
            acceptJourneyAltitude(reinterpret_cast<const uint8_t *>(value.c_str()),
                                  value.length());
        }
        if (journeyAltitudeCharacteristic && journeyAltitudeCharacteristic->canNotify())
            journeyAltitudeCharacteristic->registerForNotify(
                journeyAltitudeNotificationCallback);
        else journeyAltitudeCharacteristic = nullptr;
    }
    if (capabilities.featureFlags & MotDisplayBle::kVehicleBoardNetworkV1Feature) {
        vehicleBoardNetworkCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kVehicleBoardNetworkUuid));
        if (vehicleBoardNetworkCharacteristic && vehicleBoardNetworkCharacteristic->canRead()) {
            const String value = vehicleBoardNetworkCharacteristic->readValue();
            acceptVehicleBoardNetwork(reinterpret_cast<const uint8_t *>(value.c_str()),
                                      value.length());
        }
        if (vehicleBoardNetworkCharacteristic && vehicleBoardNetworkCharacteristic->canNotify())
            vehicleBoardNetworkCharacteristic->registerForNotify(
                vehicleBoardNetworkNotificationCallback);
        else vehicleBoardNetworkCharacteristic = nullptr;
    }
    if (capabilities.featureFlags & MotDisplayBle::kSoftwareVersionsV1Feature) {
        softwareVersionsCharacteristic = service->getCharacteristic(
            BLEUUID(MotDisplayBle::kSoftwareVersionsUuid));
        if (softwareVersionsCharacteristic && softwareVersionsCharacteristic->canRead()) {
            const String value = softwareVersionsCharacteristic->readValue();
            acceptSoftwareVersions(reinterpret_cast<const uint8_t *>(value.c_str()),
                                   value.length());
        }
        if (softwareVersionsCharacteristic && softwareVersionsCharacteristic->canNotify())
            softwareVersionsCharacteristic->registerForNotify(
                softwareVersionsNotificationCallback);
        else softwareVersionsCharacteristic = nullptr;
    }
    if (telemetryCharacteristic->canRead()) {
        const String value = telemetryCharacteristic->readValue();
        acceptPacket(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
    }
    telemetryCharacteristic->registerForNotify(notificationCallback);
    statusCharacteristic->registerForNotify(statusNotificationCallback);
    connected = true;
    securityRecoveryAttempted = false;
    if (!boundPeerAddress.length()) {
        boundPeerAddress = target->getAddress().toString();
        preferences.putString(kPeerAddressKey, boundPeerAddress);
        pairingMode = false;
        Serial.printf("BLE: paired permanently with MOT %s\n", boundPeerAddress.c_str());
    }
    statusText = "MOT VERBUNDEN";
    Serial.println("BLE: MOT telemetry connected and subscribed");
    return true;
}

void connectWorker(void *)
{
    BLEAdvertisedDevice *target = candidate;
    candidate = nullptr;
    connectPending = false;
    connectToCandidate(target);
    delete target;
    connectWorkerRunning = false;
    vTaskDelete(nullptr);
}
}  // namespace

void motBleClientSetup()
{
    preferences.begin(kPreferencesNamespace, false);
    boundPeerAddress = preferences.getString(kPeerAddressKey, "");
    pairingMode = !boundPeerAddress.length();
    pairingDeadlineMs = millis() + kInitialPairingWindowMs;
    BLEDevice::init("MOT Display");
    BLESecurity *security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setAuthenticationMode(true, false, true);
    BLEScan *scanner = BLEDevice::getScan();
    scanner->setAdvertisedDeviceCallbacks(new ScanCallbacks());
    scanner->setInterval(160);
    scanner->setWindow(80);
    scanner->setActiveScan(true);
    lastScanMs = millis() - kScanRetryMs;
    statusText = pairingMode ? "KOPPELBEREIT" : "SUCHE GEKOPPELTES MOT";
}

void motBleClientLoop()
{
    if (connectPending && !connectWorkerRunning) {
        connectWorkerRunning = true;
        BaseType_t started = xTaskCreate(connectWorker, "mot-ble-connect", 16384,
                                         nullptr, 1, nullptr);
        if (started != pdPASS) {
            connectWorkerRunning = false;
            connectPending = false;
            delete candidate;
            candidate = nullptr;
            statusText = "BLE WORKER FEHLER";
            Serial.println("BLE: connection worker could not start");
        }
        return;
    }
    if (!connected && !connectWorkerRunning && !scanRunning &&
        millis() - lastScanMs >= kScanRetryMs) startScan();
}

bool motBleClientLatest(MotDisplayBle::TelemetryV1 &packet, uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestPacket;
    receivedMs = latestPacketMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= kPacketStaleMs && MotDisplayBle::valid(packet);
}

bool motBleClientLatestStatus(MotDisplayBle::StatusV1 &packet, uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestStatus;
    receivedMs = latestStatusMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 3500U && MotDisplayBle::valid(packet);
}

bool motBleClientLatestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet, uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestExtended;
    receivedMs = latestExtendedMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 2500U && MotDisplayBle::valid(packet);
}

bool motBleClientLatestDriving(MotDisplayBle::DrivingStatusV1 &packet, uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestDriving;
    receivedMs = latestDrivingMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 2500U && MotDisplayBle::valid(packet);
}

bool motBleClientLatestChargeSummary(MotDisplayBle::ChargeSummaryV1 &packet,
                                     uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestChargeSummary;
    receivedMs = latestChargeSummaryMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 3500U && MotDisplayBle::valid(packet);
}

bool motBleClientLatestJourneyAltitude(MotDisplayBle::JourneyAltitudeV1 &packet,
                                       uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestJourneyAltitude;
    receivedMs = latestJourneyAltitudeMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 3500U && MotDisplayBle::valid(packet);
}

bool motBleClientLatestPresentationConfig(MotDisplayBle::PresentationConfigV1 &packet,
                                          uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestPresentationConfig;
    receivedMs = latestPresentationConfigMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return MotDisplayBle::valid(packet);
}

bool motBleClientLatestVehicleBoardNetwork(MotDisplayBle::VehicleBoardNetworkV1 &packet,
                                           uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestVehicleBoardNetwork;
    receivedMs = latestVehicleBoardNetworkMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 3500U && MotDisplayBle::valid(packet) &&
        packet.historyCount <= MotDisplayBle::kBoardVoltageHistorySamples;
}

bool motBleClientLatestSoftwareVersions(MotDisplayBle::SoftwareVersionsV1 &packet,
                                       uint32_t &receivedAgeMs)
{
    uint32_t receivedMs = 0;
    portENTER_CRITICAL(&packetMux);
    packet = latestSoftwareVersions;
    receivedMs = latestSoftwareVersionsMs;
    portEXIT_CRITICAL(&packetMux);
    if (!receivedMs) return false;
    receivedAgeMs = millis() - receivedMs;
    return receivedAgeMs <= 3500U && MotDisplayBle::valid(packet);
}

bool motBleClientWritePresentationConfig(uint8_t language, uint8_t unitSystem)
{
    if (!connected || !presentationConfigCharacteristic || language > 3 || unitSystem > 1)
        return false;
    MotDisplayBle::PresentationConfigV1 packet{};
    packet.magic = MotDisplayBle::kMagic;
    packet.version = MotDisplayBle::kProtocolVersion;
    packet.language = language;
    packet.unitSystem = unitSystem;
    MotDisplayBle::seal(packet);
    presentationConfigCharacteristic->writeValue(
        reinterpret_cast<uint8_t *>(&packet), sizeof(packet), true);
    return true;
}

const char *motBleClientStatus()
{
    return statusText;
}

bool motBleClientIsBound()
{
    return boundPeerAddress.length() > 0;
}

void motBleClientForgetPairing()
{
    if (client && client->isConnected()) client->disconnect();
    clearStoredBonds();
    preferences.remove(kPeerAddressKey);
    boundPeerAddress = "";
    pairingMode = true;
    pairingDeadlineMs = millis() + kInitialPairingWindowMs;
    lastScanMs = millis() - kScanRetryMs;
    statusText = "KOPPELBEREIT";
    Serial.println("BLE: fixed MOT assignment cleared; pairing window opened");
}
