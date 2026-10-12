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

namespace {
constexpr uint32_t kScanRetryMs = 5000;
constexpr uint32_t kPacketStaleMs = 2500;
constexpr uint32_t kScanDurationSeconds = 3;
constexpr uint32_t kInitialPairingWindowMs = 120000;
constexpr char kPreferencesNamespace[] = "mot-display";
constexpr char kPeerAddressKey[] = "peer";

BLEAdvertisedDevice *candidate = nullptr;
BLEClient *client = nullptr;
BLERemoteCharacteristic *telemetryCharacteristic = nullptr;
BLERemoteCharacteristic *statusCharacteristic = nullptr;
BLERemoteCharacteristic *extendedTelemetryCharacteristic = nullptr;
BLERemoteCharacteristic *drivingStatusCharacteristic = nullptr;
bool connected = false;
bool connectPending = false;
volatile bool scanRunning = false;
bool pairingMode = false;
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
portMUX_TYPE packetMux = portMUX_INITIALIZER_UNLOCKED;
const char *statusText = "SUCHE MOT";

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

class ClientCallbacks final : public BLEClientCallbacks {
    void onConnect(BLEClient *) override {}

    void onDisconnect(BLEClient *) override
    {
        connected = false;
        telemetryCharacteristic = nullptr;
        statusCharacteristic = nullptr;
        extendedTelemetryCharacteristic = nullptr;
        drivingStatusCharacteristic = nullptr;
        statusText = "VERBINDUNG WEG";
        Serial.println("BLE: MOT disconnected");
    }
};

class ScanCallbacks final : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override
    {
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

bool connectToCandidate()
{
    connectPending = false;
    if (!candidate) return false;
    if (!client) {
        client = BLEDevice::createClient();
        client->setClientCallbacks(new ClientCallbacks());
    }
    if (!client->connect(candidate)) {
        statusText = "MOT NICHT ERREICHBAR";
        return false;
    }
    if (!client->secureConnection()) {
        client->disconnect();
        statusText = "KOPPLUNG FEHLER";
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
    if (telemetryCharacteristic->canRead()) {
        const String value = telemetryCharacteristic->readValue();
        acceptPacket(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
    }
    telemetryCharacteristic->registerForNotify(notificationCallback);
    statusCharacteristic->registerForNotify(statusNotificationCallback);
    connected = true;
    if (!boundPeerAddress.length()) {
        boundPeerAddress = candidate->getAddress().toString();
        preferences.putString(kPeerAddressKey, boundPeerAddress);
        pairingMode = false;
        Serial.printf("BLE: paired permanently with MOT %s\n", boundPeerAddress.c_str());
    }
    statusText = "MOT VERBUNDEN";
    Serial.println("BLE: MOT telemetry connected and subscribed");
    return true;
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
    if (connectPending) {
        connectToCandidate();
        delete candidate;
        candidate = nullptr;
        return;
    }
    if (!connected && !scanRunning && millis() - lastScanMs >= kScanRetryMs) startScan();
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
    preferences.remove(kPeerAddressKey);
    boundPeerAddress = "";
    pairingMode = true;
    pairingDeadlineMs = millis() + kInitialPairingWindowMs;
    lastScanMs = millis() - kScanRetryMs;
    statusText = "KOPPELBEREIT";
    Serial.println("BLE: fixed MOT assignment cleared; pairing window opened");
}
