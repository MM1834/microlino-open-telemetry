#pragma once

#include <stddef.h>
#include <stdint.h>

namespace MotDisplayBle {

constexpr char kServiceUuid[] = "91bad492-b950-4226-aa2b-4ede9fa42f59";
constexpr char kTelemetryUuid[] = "cba1d466-344c-4be3-ab3f-189f80dd7518";
constexpr char kStatusUuid[] = "d8f3f4c2-7795-4c3d-9e71-33f4f6c0b112";
constexpr char kCapabilitiesUuid[] = "66c17b7e-21e4-4bc2-9d4d-bf6ff52c4d01";
constexpr char kExtendedTelemetryUuid[] = "e417d275-18c1-4bc5-8f8e-4c864f6061d1";
constexpr char kDrivingStatusUuid[] = "6f6572db-4ee8-4e8b-91cb-356e86f97d3a";
constexpr uint16_t kMagic = 0x4D54;  // "MT" on the wire, little endian.
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kCapabilitiesVersion = 1;

enum ScreenFlags : uint8_t {
    kPowerScreen = 1U << 0,
    kAdapterStatusScreen = 1U << 1,
};

enum FeatureFlags : uint16_t {
    kTelemetryV1Feature = 1U << 0,
    kStatusV1Feature = 1U << 1,
    kBondedEncryptionFeature = 1U << 2,
    kExtendedTelemetryV1Feature = 1U << 3,
    kDrivingStatusV1Feature = 1U << 4,
};

enum DrivingFlags : uint16_t {
    kDrivingSpeedValid = 1U << 0,
    kIndicatorStateValid = 1U << 1,
    kLeftIndicatorActive = 1U << 2,
    kRightIndicatorActive = 1U << 3,
    kSafetyStateValid = 1U << 4,
    kParkingBrakeActive = 1U << 5,
    kWarning90Active = 1U << 6,
    kWarningHblAhActive = 1U << 7,
    kDrivetrainWarningYellow = 1U << 8,
    // Compact additive states in the formerly unused upper flag bits. Zero is
    // unknown so older adapters remain fail-closed on newer displays.
    kGearStateBit0 = 1U << 9,
    kGearStateBit1 = 1U << 10,
    kLightStateBit0 = 1U << 11,
    kLightStateBit1 = 1U << 12,
    kDriveModeBit0 = 1U << 13,
    kDriveModeBit1 = 1U << 14,
    kCandidateStatesValid = 1U << 15,
};

constexpr uint8_t kGearStateShift = 9;
constexpr uint16_t kGearStateMask = 0x3U << kGearStateShift;
constexpr uint8_t kLightStateShift = 11;
constexpr uint16_t kLightStateMask = 0x3U << kLightStateShift;
constexpr uint8_t kDriveModeShift = 13;
constexpr uint16_t kDriveModeMask = 0x3U << kDriveModeShift;

enum CompactGearState : uint8_t {
    kGearNeutral = 0,
    kGearDrive = 1,
    kGearReverse = 2,
    kGearUnknown = 3,
};

enum CompactLightState : uint8_t {
    kLightOff = 0,
    kLightBit1 = 1,
    kLightBit2 = 2,
    kLightBits1And2 = 3,
};

enum CompactDriveMode : uint8_t {
    kDriveModeNormal = 0,
    kDriveModeEco = 1,
    kDriveModeSport = 2,
    kDriveModeConflict = 3,
};

enum ExtendedFlags : uint16_t {
    kBatteryValuesValid = 1U << 0,
    kSohValid = 1U << 1,
    kCellDeltaValid = 1U << 2,
    kJourneyValid = 1U << 3,
    kJourneyConsumptionValid = 1U << 4,
    kRegenPeakValid = 1U << 5,
    kChargeValid = 1U << 6,
    kChargeEnergyValid = 1U << 7,
    kChargeTargetValid = 1U << 8,
    kChargeRemainingValid = 1U << 9,
};

enum Flags : uint8_t {
    kDataValid = 1U << 0,
    kSocValid = 1U << 1,
    kRangeValid = 1U << 2,
    kCharging = 1U << 3,
    kRegenerating = 1U << 4,
    kConsuming = 1U << 5,
};

enum StatusFlags : uint8_t {
    kWifi1Configured = 1U << 0,
    kWifi2Configured = 1U << 1,
    kNetworkOnline = 1U << 2,
    kWifi1Active = 1U << 3,
    kWifi2Active = 1U << 4,
    kAwsEnabled = 1U << 5,
    kAwsConnected = 1U << 6,
    kAwsCredentialsReady = 1U << 7,
};

enum StatusFlags2 : uint8_t {
    kAccessPointActive = 1U << 0,
    // A telemetry publish completed within the adapter's bounded health window.
    // This is additive: older displays ignore the previously unused bit.
    kAwsRecentlyHealthy = 1U << 1,
};

#pragma pack(push, 1)
struct TelemetryV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t sequence;
    int16_t powerDeciKw;
    uint8_t socPercent;
    uint8_t targetSocPercent;
    int16_t rangeToTargetKm;
    uint16_t sourceAgeMs;
    uint16_t crc16;
};

struct StatusV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t sequence;
    int8_t wifiRssi;
    uint8_t flags2;
    char wifi1Name[17];
    char wifi2Name[17];
    char ipAddress[16];
    uint16_t crc16;
};

// Additive release contract: this record advertises compatible protocol and
// feature ranges. TelemetryV1 and StatusV1 remain byte-for-byte stable.
struct CapabilitiesV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t protocolMin;
    uint8_t protocolMax;
    uint8_t screenFlags;
    uint16_t featureFlags;
    uint16_t crc16;
};

struct ExtendedTelemetryV1 {
    uint16_t magic;
    uint8_t version;
    uint16_t flags;
    uint16_t sequence;
    uint16_t sohHundredths;
    uint16_t packVoltageDeciV;
    int16_t packCurrentDeciA;
    uint16_t cellDeltaMv;
    uint16_t journeyDistanceDeciKm;
    uint16_t journeyDurationMinutes;
    int16_t journeyConsumptionDeciKwhPer100Km;
    uint16_t regenPeakDeciKw;
    uint16_t chargePowerDeciKw;
    uint16_t chargeEnergyWh;
    uint8_t chargeTargetSocPercent;
    uint16_t chargeRemainingMinutes;
    uint16_t crc16;
};

struct DrivingStatusV1 {
    uint16_t magic;
    uint8_t version;
    uint16_t flags;
    uint16_t sequence;
    uint16_t speedDeciKmh;
    uint8_t warning90Code;
    uint16_t sourceAgeMs;
    uint16_t crc16;
};
#pragma pack(pop)

static_assert(sizeof(TelemetryV1) == 16, "BLE display packet must remain 16 bytes");
static_assert(sizeof(StatusV1) == 60, "BLE display status packet must remain 60 bytes");
static_assert(sizeof(CapabilitiesV1) == 10, "BLE capability packet must remain 10 bytes");
static_assert(sizeof(ExtendedTelemetryV1) == 32, "BLE extended packet must remain 32 bytes");
static_assert(sizeof(DrivingStatusV1) == 14, "BLE driving packet must be 14 bytes");

inline uint16_t crc16Ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t index = 0; index < length; ++index) {
        crc ^= static_cast<uint16_t>(data[index]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                                  : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

inline void seal(TelemetryV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(TelemetryV1, crc16));
}

inline bool valid(const TelemetryV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(TelemetryV1, crc16));
}

inline void seal(StatusV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(StatusV1, crc16));
}

inline bool valid(const StatusV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(StatusV1, crc16));
}

inline void seal(CapabilitiesV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(CapabilitiesV1, crc16));
}

inline bool valid(const CapabilitiesV1 &packet)
{
    return packet.magic == kMagic && packet.version == kCapabilitiesVersion &&
        packet.protocolMin <= kProtocolVersion && packet.protocolMax >= kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(CapabilitiesV1, crc16));
}

inline void seal(ExtendedTelemetryV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(ExtendedTelemetryV1, crc16));
}

inline bool valid(const ExtendedTelemetryV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(ExtendedTelemetryV1, crc16));
}

inline void seal(DrivingStatusV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(DrivingStatusV1, crc16));
}

inline bool valid(const DrivingStatusV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(DrivingStatusV1, crc16));
}

}  // namespace MotDisplayBle
