#include "c6_bm2_diag.h"

#ifdef MOT_BM2_DIAGNOSTIC

#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <mbedtls/aes.h>

#include <cstring>

namespace {
constexpr uint32_t kInitialScanDelayMs = 30000;
constexpr uint32_t kScanIntervalMs = 60000;
constexpr uint32_t kScanDurationSeconds = 15;
constexpr uint16_t kBleScanIntervalMs = 1000;
constexpr uint16_t kBleScanWindowMs = 200;
constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kAutoCaptureTimeoutMs = 60000;
constexpr uint32_t kAutoHistoryDelayMs = 5000;
constexpr uint32_t kAutoDisconnectDelayMs = 3000;
constexpr uint8_t kAutoMaximumConnectionAttempts = 3;
const BLEUUID kBm2ServiceUuid("0000fff0-0000-1000-8000-00805f9b34fb");
const BLEUUID kBm2NotifyUuid("0000fff4-0000-1000-8000-00805f9b34fb");
const BLEUUID kBm2WriteUuid("0000fff3-0000-1000-8000-00805f9b34fb");
constexpr size_t kHistoryBufferSize = 8192;
constexpr uint32_t kHistorySecondCommandDelayMs = 1000;

constexpr uint8_t kBm2Key[16] = {
    108, 101, 97, 103, 101, 110, 100, 255,
    254, 49, 56, 56, 50, 52, 54, 54,
};
constexpr uint8_t kBm6Key[16] = {
    108, 101, 97, 103, 101, 110, 100, 255,
    254, 48, 49, 48, 48, 48, 48, 57,
};

enum class MonitorProtocol : uint8_t { None, Bm2, Bm6 };
enum class AutoDiagnosticPhase : uint8_t {
    WaitingForScan, CandidateFound, Connecting, Capturing,
    RequestingHistory, Complete, Failed,
};

BLEScan *scanner = nullptr;
uint32_t lastScanStartMs = 0;
uint32_t scanStarts = 0;
uint32_t advertisements = 0;
uint32_t decodedFrames = 0;
uint32_t rejectedFrames = 0;
uint32_t candidateFrames = 0;
uint32_t lastSeenMs = 0;
float lastVoltage = 0.0f;
uint8_t lastCharge = 0;
int lastRssi = 0;
char lastAddress[24] = "";
char lastName[40] = "";
char lastCandidateAddress[24] = "";
char lastCandidateName[40] = "";
char lastCandidateManufacturerHex[65] = "";
size_t lastCandidateManufacturerLength = 0;
bool lastCandidateAdvertisesFff0 = false;
uint8_t lastCandidateAddressType = 0;
char bm6CandidateAddress[24] = "";
uint8_t bm6CandidateAddressType = 0;
char bm2CandidateAddress[24] = "";
uint8_t bm2CandidateAddressType = 0;
BLEClient *client = nullptr;
BLERemoteCharacteristic *writeCharacteristic = nullptr;
bool subscribed = false;
uint32_t connectionAttempts = 0;
uint32_t connectionFailures = 0;
uint32_t notifications = 0;
char connectionMessage[72] = "not connected";
MonitorProtocol connectedProtocol = MonitorProtocol::None;
float lastTemperatureC = 0.0f;
bool lastTemperatureValid = false;
uint8_t historyData[kHistoryBufferSize];
size_t historyDataLength = 0;
uint32_t historyExpectedSamples = 0;
uint32_t historyCountReceivedMs = 0;
uint32_t historySamples = 0;
float historyLatestVoltage = 0.0f;
float historyMinimumVoltage = 0.0f;
float historyMaximumVoltage = 0.0f;
float historyMinimumCrankVoltage = 0.0f;
uint32_t historyBelow118Samples = 0;
uint32_t historyBelow120Samples = 0;
bool historyReceiving = false;
bool historySecondCommandPending = false;
bool historyComplete = false;
char historyMessage[80] = "not requested";
AutoDiagnosticPhase autoPhase = AutoDiagnosticPhase::WaitingForScan;
MonitorProtocol autoProtocol = MonitorProtocol::None;
uint8_t autoConnectionAttempts = 0;
uint32_t autoConnectionStartedMs = 0;
uint32_t autoCompletedMs = 0;
uint32_t autoNextAttemptMs = 0;
uint32_t autoNotificationBaseline = 0;
bool autoHistoryRequested = false;
char autoMessage[112] = "waiting for BM2/BM6; close the smartphone app so the monitor is available";
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

const char *autoPhaseName(AutoDiagnosticPhase phase)
{
    switch (phase) {
        case AutoDiagnosticPhase::WaitingForScan: return "waiting_for_scan";
        case AutoDiagnosticPhase::CandidateFound: return "candidate_found";
        case AutoDiagnosticPhase::Connecting: return "connecting";
        case AutoDiagnosticPhase::Capturing: return "capturing";
        case AutoDiagnosticPhase::RequestingHistory: return "requesting_history";
        case AutoDiagnosticPhase::Complete: return "complete";
        case AutoDiagnosticPhase::Failed: return "failed";
    }
    return "unknown";
}

String jsonEscape(const char *value)
{
    String escaped;
    for (const char *cursor = value; cursor && *cursor; ++cursor) {
        if (*cursor == '\\' || *cursor == '"') escaped += '\\';
        if (static_cast<uint8_t>(*cursor) >= 0x20) escaped += *cursor;
    }
    return escaped;
}

bool decodeAdvertisement(const String &manufacturerData, float &voltage, uint8_t &charge)
{
    // Arduino BLE returns the manufacturer identifier plus its 14 data bytes.
    // A BM2 advertisement therefore forms exactly one AES-128-CBC block.
    if (manufacturerData.length() != 16) return false;

    uint8_t input[16];
    uint8_t output[16];
    uint8_t iv[16] = {};
    memcpy(input, manufacturerData.c_str(), sizeof(input));

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const int keyResult = mbedtls_aes_setkey_dec(&aes, kBm2Key, 128);
    const int decryptResult = keyResult == 0
        ? mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, sizeof(input), iv, input, output)
        : keyResult;
    mbedtls_aes_free(&aes);
    if (decryptResult != 0) return false;

    voltage = static_cast<float>((static_cast<uint16_t>(output[6]) << 8) | output[7]) / 100.0f;
    charge = output[8];
    return voltage >= 6.0f && voltage <= 18.0f && charge <= 100;
}

bool decryptBlock(const uint8_t key[16], const uint8_t *input, size_t length, uint8_t output[16])
{
    if (!input || length != 16) return false;
    uint8_t iv[16] = {};
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const int keyResult = mbedtls_aes_setkey_dec(&aes, key, 128);
    const int decryptResult = keyResult == 0
        ? mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv, input, output)
        : keyResult;
    mbedtls_aes_free(&aes);
    return decryptResult == 0;
}

bool encryptCommand(const uint8_t key[16], const uint8_t *input, size_t length, uint8_t output[16])
{
    if (!input || length == 0 || length > 16) return false;
    uint8_t padded[16] = {};
    uint8_t iv[16] = {};
    memcpy(padded, input, length);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const int keyResult = mbedtls_aes_setkey_enc(&aes, key, 128);
    const int encryptResult = keyResult == 0
        ? mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, sizeof(padded), iv, padded, output)
        : keyResult;
    mbedtls_aes_free(&aes);
    return encryptResult == 0;
}

bool writeEncryptedCommand(const uint8_t *command, size_t length)
{
    uint8_t encrypted[16];
    return writeCharacteristic && encryptCommand(kBm2Key, command, length, encrypted) &&
        writeCharacteristic->writeValue(encrypted, sizeof(encrypted), false);
}

uint32_t decodeThreeBytes(const uint8_t *value)
{
    return (static_cast<uint32_t>(value[0]) << 16) |
        (static_cast<uint32_t>(value[1]) << 8) | value[2];
}

uint16_t nibbleValue(const uint8_t *value, size_t startNibble, size_t nibbleCount)
{
    uint16_t result = 0;
    for (size_t i = 0; i < nibbleCount; ++i) {
        const size_t nibble = startNibble + i;
        const uint8_t part = nibble % 2 == 0 ? value[nibble / 2] >> 4 : value[nibble / 2] & 0x0F;
        result = static_cast<uint16_t>((result << 4) | part);
    }
    return result;
}

void summarizeHistory()
{
    historySamples = historyDataLength / 4;
    historyLatestVoltage = 0.0f;
    historyMinimumVoltage = 0.0f;
    historyMaximumVoltage = 0.0f;
    historyMinimumCrankVoltage = 0.0f;
    historyBelow118Samples = 0;
    historyBelow120Samples = 0;
    for (uint32_t i = 0; i < historySamples; ++i) {
        const uint8_t *sample = &historyData[i * 4];
        const float voltage = nibbleValue(sample, 0, 3) / 100.0f;
        const float crank = nibbleValue(sample, 4, 3) / 100.0f;
        if (i == 0 || voltage < historyMinimumVoltage) historyMinimumVoltage = voltage;
        if (i == 0 || voltage > historyMaximumVoltage) historyMaximumVoltage = voltage;
        if (crank >= 6.0f && crank <= 18.0f &&
            (historyMinimumCrankVoltage == 0.0f || crank < historyMinimumCrankVoltage)) {
            historyMinimumCrankVoltage = crank;
        }
        if (voltage < 11.8f) ++historyBelow118Samples;
        if (voltage < 12.0f) ++historyBelow120Samples;
        historyLatestVoltage = voltage;
    }
    historyComplete = true;
    snprintf(historyMessage, sizeof(historyMessage), "complete: %lu samples (%lu min)",
             static_cast<unsigned long>(historySamples),
             static_cast<unsigned long>(historySamples * 2));
    Serial.printf("BM2 history complete: samples=%lu span=%lu min latest=%.2f V min=%.2f V max=%.2f V minCrank=%.2f V below11.8=%lu min\n",
                  static_cast<unsigned long>(historySamples),
                  static_cast<unsigned long>(historySamples * 2), historyLatestVoltage,
                  historyMinimumVoltage, historyMaximumVoltage, historyMinimumCrankVoltage,
                  static_cast<unsigned long>(historyBelow118Samples * 2));
}

void notifyCallback(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    uint8_t decoded[16];
    const uint8_t *key = connectedProtocol == MonitorProtocol::Bm6 ? kBm6Key : kBm2Key;
    if (!decryptBlock(key, data, length, decoded)) return;

    if (connectedProtocol == MonitorProtocol::Bm6) {
        if (decoded[0] != 0xD1 || decoded[1] != 0x55 || decoded[2] != 0x07) return;
        const float voltage = nibbleValue(decoded, 15, 3) / 100.0f;
        const int temperature = decoded[3] == 0x01 ? -static_cast<int>(decoded[4]) : decoded[4];
        const uint8_t soc = decoded[6];
        if (voltage < 6.0f || voltage > 18.0f || soc > 100 || temperature < -60 || temperature > 100) return;
        portENTER_CRITICAL(&stateMux);
        ++notifications;
        ++decodedFrames;
        lastSeenMs = millis();
        lastVoltage = voltage;
        lastCharge = soc;
        lastTemperatureC = static_cast<float>(temperature);
        lastTemperatureValid = true;
        strlcpy(lastAddress, bm6CandidateAddress, sizeof(lastAddress));
        strlcpy(lastName, "BM6", sizeof(lastName));
        strlcpy(connectionMessage, "BM6 FFF4 notification received", sizeof(connectionMessage));
        portEXIT_CRITICAL(&stateMux);
        Serial.printf("BM6 FFF4: voltage=%.2f V temperature=%d C soc=%u%%\n",
                      voltage, temperature, static_cast<unsigned>(soc));
        return;
    }

    if (decoded[0] == 0xE7) {
        const uint32_t expected = decodeThreeBytes(&decoded[1]);
        historyExpectedSamples = expected;
        historyCountReceivedMs = millis();
        historySecondCommandPending = expected > 0 && expected <= kHistoryBufferSize / 4;
        historyComplete = expected == 0;
        strlcpy(historyMessage,
                expected == 0 ? "complete: no stored samples" :
                expected <= kHistoryBufferSize / 4 ? "count received; awaiting data request" :
                "history exceeds diagnostic buffer",
                sizeof(historyMessage));
        Serial.printf("BM2 history count: %lu samples\n", static_cast<unsigned long>(expected));
        return;
    }
    if (decoded[0] == 0xFF && decoded[1] == 0xFF && decoded[2] == 0xFE) {
        historyDataLength = 0;
        historyReceiving = true;
        strlcpy(historyMessage, "receiving", sizeof(historyMessage));
        Serial.println("BM2 history transfer started");
        return;
    }
    if (historyReceiving) {
        if (decoded[0] == 0xFF && decoded[1] == 0xFE && decoded[2] == 0xFE) {
            historyReceiving = false;
            const uint32_t framedSize = decodeThreeBytes(&decoded[3]);
            const size_t usefulSize = framedSize >= 9 ? framedSize - 9 : 0;
            if (usefulSize < historyDataLength) historyDataLength = usefulSize;
            historyDataLength -= historyDataLength % 4;
            summarizeHistory();
            return;
        }
        const size_t room = kHistoryBufferSize - historyDataLength;
        const size_t copyLength = length < room ? length : room;
        memcpy(&historyData[historyDataLength], decoded, copyLength);
        historyDataLength += copyLength;
        if (copyLength < length) {
            historyReceiving = false;
            strlcpy(historyMessage, "history buffer overflow", sizeof(historyMessage));
        }
        return;
    }
    if (decoded[0] != 0xF5) return;

    const uint16_t packed = (static_cast<uint16_t>(decoded[1]) << 8) | decoded[2];
    const float voltage = static_cast<float>(packed >> 4) / 100.0f;
    if (voltage < 6.0f || voltage > 18.0f) return;

    portENTER_CRITICAL(&stateMux);
    ++notifications;
    ++decodedFrames;
    lastSeenMs = millis();
    lastVoltage = voltage;
    lastCharge = 0;
    strlcpy(lastAddress, lastCandidateAddress, sizeof(lastAddress));
    strlcpy(lastName, lastCandidateName, sizeof(lastName));
    strlcpy(connectionMessage, "FFF4 voltage notification received", sizeof(connectionMessage));
    portEXIT_CRITICAL(&stateMux);
    Serial.printf("BM2 FFF4 voltage: %.2f V\n", voltage);
}

class Bm2ClientCallbacks final : public BLEClientCallbacks {
public:
    void onDisconnect(BLEClient *) override
    {
        portENTER_CRITICAL(&stateMux);
        subscribed = false;
        writeCharacteristic = nullptr;
        connectedProtocol = MonitorProtocol::None;
        strlcpy(connectionMessage, "disconnected", sizeof(connectionMessage));
        portEXIT_CRITICAL(&stateMux);
        Serial.println("BM2 GATT disconnected");
    }
};

Bm2ClientCallbacks clientCallbacks;

String lowerCopy(String value)
{
    value.toLowerCase();
    return value;
}

String hexBytes(const String &value)
{
    static const char digits[] = "0123456789ABCDEF";
    String out;
    const size_t length = value.length() > 32 ? 32 : value.length();
    out.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        const uint8_t byte = static_cast<uint8_t>(value.charAt(i));
        out += digits[byte >> 4];
        out += digits[byte & 0x0F];
    }
    return out;
}

class Bm2AdvertisedDeviceCallbacks final : public BLEAdvertisedDeviceCallbacks {
public:
    void onResult(BLEAdvertisedDevice device) override
    {
        portENTER_CRITICAL(&stateMux);
        ++advertisements;
        portEXIT_CRITICAL(&stateMux);
        const String address = device.getAddress().toString();
        const String name = device.haveName() ? device.getName() : String("unknown");
        const String lowerName = lowerCopy(name);
        const String manufacturerData = device.haveManufacturerData()
            ? device.getManufacturerData() : String();
        const bool advertisesFff0 = device.isAdvertisingService(BLEUUID("fff0"));
        const bool likelyName = lowerName.indexOf("battery") >= 0 ||
            lowerName.indexOf("ancel") >= 0 || lowerName.indexOf("bm2") >= 0 ||
            lowerName.indexOf("bm6") >= 0;
        const bool bm6Name = lowerName == "bm6" || lowerName.startsWith("bm6 ");
        const bool bm2GattCandidate = !bm6Name && advertisesFff0;
        const bool candidate = likelyName || advertisesFff0 || manufacturerData.length() == 16;

        if (candidate) {
            const String manufacturerHex = hexBytes(manufacturerData);
            bool printCandidate = false;
            portENTER_CRITICAL(&stateMux);
            ++candidateFrames;
            printCandidate = strcmp(lastCandidateAddress, address.c_str()) != 0 ||
                strcmp(lastCandidateName, name.c_str()) != 0 ||
                strcmp(lastCandidateManufacturerHex, manufacturerHex.c_str()) != 0 ||
                lastCandidateAdvertisesFff0 != advertisesFff0;
            strlcpy(lastCandidateAddress, address.c_str(), sizeof(lastCandidateAddress));
            strlcpy(lastCandidateName, name.c_str(), sizeof(lastCandidateName));
            strlcpy(lastCandidateManufacturerHex, manufacturerHex.c_str(),
                    sizeof(lastCandidateManufacturerHex));
            lastCandidateManufacturerLength = manufacturerData.length();
            lastCandidateAdvertisesFff0 = advertisesFff0;
            lastCandidateAddressType = device.getAddressType();
            if (bm6Name) {
                strlcpy(bm6CandidateAddress, address.c_str(), sizeof(bm6CandidateAddress));
                bm6CandidateAddressType = device.getAddressType();
            } else if (bm2GattCandidate && !bm2CandidateAddress[0]) {
                // Retain the first non-BM6 FFF0 device for the bounded automatic
                // run instead of letting unrelated later advertisements replace it.
                strlcpy(bm2CandidateAddress, address.c_str(), sizeof(bm2CandidateAddress));
                bm2CandidateAddressType = device.getAddressType();
            }
            portEXIT_CRITICAL(&stateMux);
            if (printCandidate) {
                Serial.printf("BM2 candidate: address=%s name=%s rssi=%d fff0=%s manufacturerLen=%u manufacturer=%s\n",
                              address.c_str(), name.c_str(), device.getRSSI(),
                              advertisesFff0 ? "yes" : "no",
                              static_cast<unsigned>(manufacturerData.length()),
                              manufacturerHex.c_str());
            }
        }

        if (!device.haveManufacturerData()) return;

        float voltage = 0.0f;
        uint8_t charge = 0;
        if (!decodeAdvertisement(manufacturerData, voltage, charge)) {
            if (likelyName) {
                portENTER_CRITICAL(&stateMux);
                ++rejectedFrames;
                portEXIT_CRITICAL(&stateMux);
            }
            return;
        }

        portENTER_CRITICAL(&stateMux);
        ++decodedFrames;
        lastSeenMs = millis();
        lastVoltage = voltage;
        lastCharge = charge;
        lastRssi = device.getRSSI();
        strlcpy(lastAddress, address.c_str(), sizeof(lastAddress));
        strlcpy(lastName, name.c_str(), sizeof(lastName));
        portEXIT_CRITICAL(&stateMux);

        Serial.printf("BM2 decoded: address=%s name=%s rssi=%d voltage=%.2f V charge=%u%%\n",
                      address.c_str(), name.c_str(), device.getRSSI(), voltage,
                      static_cast<unsigned>(charge));
    }
};

Bm2AdvertisedDeviceCallbacks advertisedDeviceCallbacks;

void scanComplete(BLEScanResults)
{
    if (scanner) scanner->clearResults();
    Serial.println("BM2 active identification scan completed");
}
}

void c6Bm2DiagSetup()
{
    BLEDevice::init("");
    scanner = BLEDevice::getScan();
    scanner->setActiveScan(true);
    scanner->setInterval(kBleScanIntervalMs);
    scanner->setWindow(kBleScanWindowMs);
    scanner->setAdvertisedDeviceCallbacks(&advertisedDeviceCallbacks, true, true);
    Serial.println("BM2/BM6 auto diagnostic ready: close the smartphone app; results appear in local Runtime diagnostics");
}

bool c6Bm2DiagStartScan()
{
    if (!scanner || scanner->isScanning() || (client && client->isConnected())) return false;
    scanner->clearResults();
    if (!scanner->start(kScanDurationSeconds, scanComplete, false)) return false;
    lastScanStartMs = millis();
    ++scanStarts;
    Serial.printf("BM2 active identification scan started for %lu seconds\n",
                  static_cast<unsigned long>(kScanDurationSeconds));
    return true;
}

bool connectMonitor(const char *address, uint8_t addressType, MonitorProtocol protocol)
{
    c6Bm2DiagStopScan();
    if (!client) {
        client = BLEDevice::createClient();
        client->setClientCallbacks(&clientCallbacks);
    }
    if (client->isConnected()) client->disconnect();

    const char *label = protocol == MonitorProtocol::Bm6 ? "BM6" : "BM2";
    Serial.printf("%s GATT connecting to %s\n", label, address);
    if (!client->connect(BLEAddress(String(address), addressType), addressType, kConnectTimeoutMs)) {
        ++connectionFailures;
        strlcpy(connectionMessage, "GATT connection failed", sizeof(connectionMessage));
        return false;
    }
    BLERemoteService *service = client->getService(kBm2ServiceUuid);
    BLERemoteCharacteristic *notify = service ? service->getCharacteristic(kBm2NotifyUuid) : nullptr;
    writeCharacteristic = service ? service->getCharacteristic(kBm2WriteUuid) : nullptr;
    if (!notify || !notify->canNotify()) {
        ++connectionFailures;
        strlcpy(connectionMessage, "FFF4 notify characteristic unavailable", sizeof(connectionMessage));
        client->disconnect();
        return false;
    }
#if defined(CONFIG_NIMBLE_ENABLED)
    subscribed = notify->subscribe(true, notifyCallback, true);
#else
    notify->registerForNotify(notifyCallback, true);
    subscribed = true;
#endif
    if (!subscribed) {
        ++connectionFailures;
        strlcpy(connectionMessage, "FFF4 subscription failed", sizeof(connectionMessage));
        client->disconnect();
        return false;
    }
    connectedProtocol = protocol;
    lastTemperatureValid = false;
    if (protocol == MonitorProtocol::Bm6) {
        const uint8_t activation[16] = {0xD1, 0x55, 0x07};
        uint8_t encrypted[16];
        if (!writeCharacteristic || !writeCharacteristic->canWrite() ||
            !encryptCommand(kBm6Key, activation, sizeof(activation), encrypted) ||
            !writeCharacteristic->writeValue(encrypted, sizeof(encrypted), true)) {
            ++connectionFailures;
            strlcpy(connectionMessage, "BM6 activation failed", sizeof(connectionMessage));
            client->disconnect();
            return false;
        }
        strlcpy(connectionMessage, "BM6 activated; awaiting FFF4", sizeof(connectionMessage));
        Serial.println("BM6 GATT connected; activation query sent and FFF4 subscribed");
    } else {
        strlcpy(connectionMessage, "BM2 connected; awaiting FFF4 voltage", sizeof(connectionMessage));
        Serial.println("BM2 GATT connected; subscribed to FFF4 without writing commands");
    }
    return true;
}

bool c6Bm2DiagConnect()
{
    char address[sizeof(lastCandidateAddress)];
    uint8_t addressType;
    bool advertisesFff0;
    portENTER_CRITICAL(&stateMux);
    strlcpy(address, lastCandidateAddress, sizeof(address));
    addressType = lastCandidateAddressType;
    advertisesFff0 = lastCandidateAdvertisesFff0;
    ++connectionAttempts;
    portEXIT_CRITICAL(&stateMux);
    if (!address[0] || !advertisesFff0) {
        strlcpy(connectionMessage, "no scanned FFF0 candidate", sizeof(connectionMessage));
        Serial.println("BM2 connect requires a scanned FFF0 candidate");
        return false;
    }
    return connectMonitor(address, addressType, MonitorProtocol::Bm2);
}

bool c6Bm6DiagConnect()
{
    char address[sizeof(bm6CandidateAddress)];
    uint8_t addressType;
    portENTER_CRITICAL(&stateMux);
    strlcpy(address, bm6CandidateAddress, sizeof(address));
    addressType = bm6CandidateAddressType;
    ++connectionAttempts;
    portEXIT_CRITICAL(&stateMux);
    if (!address[0]) {
        strlcpy(connectionMessage, "no scanned BM6 candidate", sizeof(connectionMessage));
        Serial.println("BM6 connect requires a scanned device named BM6");
        return false;
    }
    return connectMonitor(address, addressType, MonitorProtocol::Bm6);
}

void c6Bm2DiagDisconnect()
{
    if (client && client->isConnected()) client->disconnect();
    subscribed = false;
    connectedProtocol = MonitorProtocol::None;
}

bool c6Bm2DiagRequestHistory()
{
    if (connectedProtocol != MonitorProtocol::Bm2 || !client || !client->isConnected() || !subscribed || !writeCharacteristic ||
        !writeCharacteristic->canWrite()) {
        strlcpy(historyMessage, "connect first or FFF3 unavailable", sizeof(historyMessage));
        return false;
    }
    const uint8_t request[] = {0xE7, 0x01};
    historyDataLength = 0;
    historyExpectedSamples = 0;
    historySamples = 0;
    historyReceiving = false;
    historySecondCommandPending = false;
    historyComplete = false;
    strlcpy(historyMessage, "requesting count", sizeof(historyMessage));
    if (!writeEncryptedCommand(request, sizeof(request))) {
        strlcpy(historyMessage, "count request failed", sizeof(historyMessage));
        return false;
    }
    Serial.println("BM2 history count requested (no configuration write)");
    return true;
}

void c6Bm2DiagDumpHistory()
{
    if (!historyComplete) {
        Serial.println("BM2 history is not complete");
        return;
    }
    const uint32_t now = static_cast<uint32_t>(time(nullptr));
    Serial.println("BM2_HISTORY index,utc,voltage_v,min_crank_v,type");
    for (uint32_t i = 0; i < historySamples; ++i) {
        const uint8_t *sample = &historyData[i * 4];
        const float voltage = nibbleValue(sample, 0, 3) / 100.0f;
        const float crank = nibbleValue(sample, 4, 3) / 100.0f;
        const uint8_t type = static_cast<uint8_t>(nibbleValue(sample, 7, 1));
        const uint32_t ageSeconds = (historySamples - 1 - i) * 120;
        Serial.printf("BM2_HISTORY %lu,%lu,%.2f,%.2f,%u\n",
                      static_cast<unsigned long>(i),
                      static_cast<unsigned long>(now > ageSeconds ? now - ageSeconds : 0),
                      voltage, crank, static_cast<unsigned>(type));
    }
}

void c6Bm2DiagStopScan()
{
    if (scanner && scanner->isScanning()) {
        scanner->stop();
        scanner->clearResults();
        Serial.println("BM2 passive BLE scan stopped");
    }
}

void c6Bm2DiagLoop()
{
    const uint32_t now = millis();
    if (historySecondCommandPending && now - historyCountReceivedMs >= kHistorySecondCommandDelayMs) {
        historySecondCommandPending = false;
        const uint8_t request[] = {
            0xE3, 0x00, 0x00,
            static_cast<uint8_t>(historyExpectedSamples >> 24),
            static_cast<uint8_t>(historyExpectedSamples >> 16),
            static_cast<uint8_t>(historyExpectedSamples >> 8),
            static_cast<uint8_t>(historyExpectedSamples),
        };
        if (writeEncryptedCommand(request, sizeof(request))) {
            strlcpy(historyMessage, "data requested", sizeof(historyMessage));
            Serial.println("BM2 history data requested");
        } else {
            strlcpy(historyMessage, "data request failed", sizeof(historyMessage));
        }
    }
    const bool connected = client && client->isConnected();
    if (connected && (autoPhase == AutoDiagnosticPhase::Capturing ||
                      autoPhase == AutoDiagnosticPhase::RequestingHistory)) {
        const bool receivedNotification = notifications > autoNotificationBaseline;
        if (autoProtocol == MonitorProtocol::Bm2 && !autoHistoryRequested &&
            now - autoConnectionStartedMs >= kAutoHistoryDelayMs) {
            autoHistoryRequested = true;
            if (c6Bm2DiagRequestHistory()) {
                autoPhase = AutoDiagnosticPhase::RequestingHistory;
                strlcpy(autoMessage, "BM2 live capture active; bounded history query in progress", sizeof(autoMessage));
            } else {
                strlcpy(autoMessage, "BM2 live capture active; history query could not start", sizeof(autoMessage));
            }
        }
        const bool bm2Done = autoProtocol == MonitorProtocol::Bm2 && historyComplete;
        const bool bm6Done = autoProtocol == MonitorProtocol::Bm6 && receivedNotification &&
            now - lastSeenMs >= kAutoDisconnectDelayMs;
        if (bm2Done || bm6Done) {
            autoPhase = AutoDiagnosticPhase::Complete;
            autoCompletedMs = now;
            strlcpy(autoMessage,
                    bm2Done ? "BM2 live values and bounded history captured; connection released" :
                              "BM6 live values captured; connection released; history protocol unknown",
                    sizeof(autoMessage));
            c6Bm2DiagDisconnect();
            Serial.println(autoMessage);
            return;
        }
        if (now - autoConnectionStartedMs >= kAutoCaptureTimeoutMs) {
            autoPhase = receivedNotification ? AutoDiagnosticPhase::Complete : AutoDiagnosticPhase::Failed;
            autoCompletedMs = now;
            strlcpy(autoMessage,
                    receivedNotification ? "live values captured; diagnostic window ended and connection released" :
                                           "no valid notification within 60 seconds; close the smartphone app and reboot to retry",
                    sizeof(autoMessage));
            c6Bm2DiagDisconnect();
            Serial.println(autoMessage);
            return;
        }
        return;
    }
    if (!scanner || scanner->isScanning() || connected) return;

    if (autoPhase != AutoDiagnosticPhase::Complete && autoPhase != AutoDiagnosticPhase::Failed &&
        autoConnectionAttempts < kAutoMaximumConnectionAttempts &&
        now >= autoNextAttemptMs &&
        (bm6CandidateAddress[0] || bm2CandidateAddress[0])) {
        autoPhase = AutoDiagnosticPhase::CandidateFound;
        autoProtocol = bm6CandidateAddress[0] ? MonitorProtocol::Bm6 : MonitorProtocol::Bm2;
        ++autoConnectionAttempts;
        autoNotificationBaseline = notifications;
        autoHistoryRequested = false;
        autoPhase = AutoDiagnosticPhase::Connecting;
        strlcpy(autoMessage,
                autoProtocol == MonitorProtocol::Bm6 ? "BM6 found; opening bounded diagnostic connection" :
                                                       "BM2 found; opening bounded diagnostic connection",
                sizeof(autoMessage));
        if (autoProtocol == MonitorProtocol::Bm2) ++connectionAttempts;
        const bool connectedNow = autoProtocol == MonitorProtocol::Bm6
            ? c6Bm6DiagConnect() : connectMonitor(bm2CandidateAddress, bm2CandidateAddressType, MonitorProtocol::Bm2);
        if (connectedNow) {
            autoConnectionStartedMs = millis();
            autoPhase = AutoDiagnosticPhase::Capturing;
            strlcpy(autoMessage,
                    autoProtocol == MonitorProtocol::Bm6 ? "BM6 connected; collecting live values" :
                                                           "BM2 connected; collecting live values before history query",
                    sizeof(autoMessage));
        } else if (autoConnectionAttempts >= kAutoMaximumConnectionAttempts) {
            autoPhase = AutoDiagnosticPhase::Failed;
            autoCompletedMs = millis();
            strlcpy(autoMessage,
                    "connection failed three times; close the smartphone app and reboot to retry",
                    sizeof(autoMessage));
        } else {
            autoPhase = AutoDiagnosticPhase::WaitingForScan;
            autoNextAttemptMs = millis() + kScanIntervalMs;
            strlcpy(autoMessage,
                    "connection failed; monitor may be in use by the smartphone app; retrying after next scan",
                    sizeof(autoMessage));
        }
        return;
    }
    const bool firstScanDue = scanStarts == 0 && now >= kInitialScanDelayMs;
    const bool repeatScanDue = autoPhase != AutoDiagnosticPhase::Complete &&
        autoPhase != AutoDiagnosticPhase::Failed && scanStarts > 0 &&
        now - lastScanStartMs >= kScanIntervalMs;
    if (firstScanDue || repeatScanDue) c6Bm2DiagStartScan();
}

String c6Bm2DiagStatusJson()
{
    uint32_t localAdvertisements;
    uint32_t localDecodedFrames;
    uint32_t localRejectedFrames;
    uint32_t localCandidateFrames;
    uint32_t localLastSeenMs;
    float localVoltage;
    uint8_t localCharge;
    int localRssi;
    char localAddress[sizeof(lastAddress)];
    char localName[sizeof(lastName)];
    char localCandidateAddress[sizeof(lastCandidateAddress)];
    char localCandidateName[sizeof(lastCandidateName)];
    char localCandidateManufacturerHex[sizeof(lastCandidateManufacturerHex)];
    size_t localCandidateManufacturerLength;
    bool localCandidateAdvertisesFff0;
    uint32_t localConnectionAttempts;
    uint32_t localConnectionFailures;
    uint32_t localNotifications;
    bool localSubscribed;
    char localConnectionMessage[sizeof(connectionMessage)];
    char localHistoryMessage[sizeof(historyMessage)];
    portENTER_CRITICAL(&stateMux);
    localAdvertisements = advertisements;
    localDecodedFrames = decodedFrames;
    localRejectedFrames = rejectedFrames;
    localCandidateFrames = candidateFrames;
    localLastSeenMs = lastSeenMs;
    localVoltage = lastVoltage;
    localCharge = lastCharge;
    localRssi = lastRssi;
    strlcpy(localAddress, lastAddress, sizeof(localAddress));
    strlcpy(localName, lastName, sizeof(localName));
    strlcpy(localCandidateAddress, lastCandidateAddress, sizeof(localCandidateAddress));
    strlcpy(localCandidateName, lastCandidateName, sizeof(localCandidateName));
    strlcpy(localCandidateManufacturerHex, lastCandidateManufacturerHex,
            sizeof(localCandidateManufacturerHex));
    localCandidateManufacturerLength = lastCandidateManufacturerLength;
    localCandidateAdvertisesFff0 = lastCandidateAdvertisesFff0;
    localConnectionAttempts = connectionAttempts;
    localConnectionFailures = connectionFailures;
    localNotifications = notifications;
    localSubscribed = subscribed;
    strlcpy(localConnectionMessage, connectionMessage, sizeof(localConnectionMessage));
    strlcpy(localHistoryMessage, historyMessage, sizeof(localHistoryMessage));
    portEXIT_CRITICAL(&stateMux);

    String out = "{\"enabled\":true,\"scanning\":";
    out += scanner && scanner->isScanning() ? "true" : "false";
    out += ",\"scanStarts\":" + String(scanStarts);
    out += ",\"advertisements\":" + String(localAdvertisements);
    out += ",\"decodedFrames\":" + String(localDecodedFrames);
    out += ",\"rejectedNamedFrames\":" + String(localRejectedFrames);
    out += ",\"candidateFrames\":" + String(localCandidateFrames);
    out += ",\"candidateAddress\":\"" + jsonEscape(localCandidateAddress) + "\"";
    out += ",\"candidateName\":\"" + jsonEscape(localCandidateName) + "\"";
    out += ",\"candidateAdvertisesFff0\":" + String(localCandidateAdvertisesFff0 ? "true" : "false");
    out += ",\"candidateManufacturerLength\":" + String(localCandidateManufacturerLength);
    out += ",\"candidateManufacturerHex\":\"" + jsonEscape(localCandidateManufacturerHex) + "\"";
    out += ",\"connected\":" + String(client && client->isConnected() ? "true" : "false");
    out += ",\"subscribed\":" + String(localSubscribed ? "true" : "false");
    out += ",\"connectionAttempts\":" + String(localConnectionAttempts);
    out += ",\"connectionFailures\":" + String(localConnectionFailures);
    out += ",\"notifications\":" + String(localNotifications);
    out += ",\"connectionMessage\":\"" + jsonEscape(localConnectionMessage) + "\"";
    out += ",\"protocol\":\"";
    out += connectedProtocol == MonitorProtocol::Bm6 ? "bm6" :
        connectedProtocol == MonitorProtocol::Bm2 ? "bm2" : "none";
    out += "\"";
    out += ",\"autoDiagnostic\":{";
    out += "\"phase\":\"" + String(autoPhaseName(autoPhase)) + "\"";
    out += ",\"protocol\":\"";
    out += autoProtocol == MonitorProtocol::Bm6 ? "bm6" :
        autoProtocol == MonitorProtocol::Bm2 ? "bm2" : "none";
    out += "\"";
    out += ",\"attempts\":" + String(autoConnectionAttempts);
    out += ",\"maximumAttempts\":" + String(kAutoMaximumConnectionAttempts);
    out += ",\"captureTimeoutSeconds\":" + String(kAutoCaptureTimeoutMs / 1000);
    out += ",\"completedAgeMs\":" + String(autoCompletedMs ? millis() - autoCompletedMs : 0);
    out += ",\"exclusiveConnectionRequired\":true";
    out += ",\"message\":\"" + jsonEscape(autoMessage) + "\"}";
    out += ",\"bm2CandidateAddress\":\"" + jsonEscape(bm2CandidateAddress) + "\"";
    out += ",\"bm6CandidateAddress\":\"" + jsonEscape(bm6CandidateAddress) + "\"";
    out += ",\"history\":{";
    out += "\"complete\":" + String(historyComplete ? "true" : "false");
    out += ",\"receiving\":" + String(historyReceiving ? "true" : "false");
    out += ",\"expectedSamples\":" + String(historyExpectedSamples);
    out += ",\"samples\":" + String(historySamples);
    out += ",\"spanMinutes\":" + String(historySamples * 2);
    out += ",\"latestVoltageV\":" + String(historyLatestVoltage, 2);
    out += ",\"minimumVoltageV\":" + String(historyMinimumVoltage, 2);
    out += ",\"maximumVoltageV\":" + String(historyMaximumVoltage, 2);
    out += ",\"minimumCrankVoltageV\":" + String(historyMinimumCrankVoltage, 2);
    out += ",\"minimumCrankVoltageValid\":" + String(historyMinimumCrankVoltage >= 6.0f ? "true" : "false");
    out += ",\"minutesBelow11_8V\":" + String(historyBelow118Samples * 2);
    out += ",\"minutesBelow12_0V\":" + String(historyBelow120Samples * 2);
    out += ",\"message\":\"" + jsonEscape(localHistoryMessage) + "\"}";
    out += ",\"detected\":" + String(localDecodedFrames ? "true" : "false");
    out += ",\"address\":\"" + jsonEscape(localAddress) + "\"";
    out += ",\"name\":\"" + jsonEscape(localName) + "\"";
    out += ",\"rssi\":" + String(localRssi);
    out += ",\"voltageV\":" + String(localVoltage, 2);
    out += ",\"chargePercent\":" + String(localCharge);
    out += ",\"temperatureValid\":" + String(lastTemperatureValid ? "true" : "false");
    out += ",\"temperatureC\":" + String(lastTemperatureC, 1);
    out += ",\"lastSeenAgeMs\":" + String(localLastSeenMs ? millis() - localLastSeenMs : 0) + "}";
    return out;
}

void c6Bm2DiagPrintStatus()
{
    Serial.println("BM2: " + c6Bm2DiagStatusJson());
}

#else

void c6Bm2DiagSetup() {}
void c6Bm2DiagLoop() {}
bool c6Bm2DiagStartScan() { return false; }
void c6Bm2DiagStopScan() {}
bool c6Bm2DiagConnect() { return false; }
bool c6Bm6DiagConnect() { return false; }
void c6Bm2DiagDisconnect() {}
bool c6Bm2DiagRequestHistory() { return false; }
void c6Bm2DiagDumpHistory() {}
String c6Bm2DiagStatusJson() { return "{\"enabled\":false}"; }
void c6Bm2DiagPrintStatus() { Serial.println("BM2 diagnostic is not included in this firmware build"); }

#endif
