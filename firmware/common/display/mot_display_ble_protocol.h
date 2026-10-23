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
constexpr char kPresentationConfigUuid[] = "39a4f29d-9605-4c6d-889f-2093de91ab45";
constexpr char kChargeSummaryUuid[] = "8c8e019a-62cb-4df0-a58f-1f978ee52bd4";
constexpr char kJourneyAltitudeUuid[] = "45fac8dc-71be-489b-810b-e6f7fbfe6047";
constexpr char kVehicleBoardNetworkUuid[] = "6dc72e5f-33f2-4b28-9415-49a24972380e";
constexpr char kSoftwareVersionsUuid[] = "35a63ac8-914e-4c12-b9ba-bef1507a98d4";
constexpr uint16_t kMagic = 0x4D54;  // "MT" on the wire, little endian.
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kCapabilitiesVersion = 1;

enum ScreenFlags : uint8_t {
    kPowerScreen = 1U << 0,
    kAdapterStatusScreen = 1U << 1,
    kSoftwareVersionsScreen = 1U << 2,
};

enum FeatureFlags : uint16_t {
    kTelemetryV1Feature = 1U << 0,
    kStatusV1Feature = 1U << 1,
    kBondedEncryptionFeature = 1U << 2,
    kExtendedTelemetryV1Feature = 1U << 3,
    kDrivingStatusV1Feature = 1U << 4,
    kPresentationConfigV1Feature = 1U << 5,
    kChargeSummaryV1Feature = 1U << 6,
    kJourneyAltitudeV1Feature = 1U << 7,
    kVehicleBoardNetworkV1Feature = 1U << 8,
    kSoftwareVersionsV1Feature = 1U << 9,
};

enum VehicleBoardNetworkFlags : uint16_t {
    kTyreFrontLeftValid = 1U << 0,
    kTyreFrontRightValid = 1U << 1,
    kTyreRearLeftValid = 1U << 2,
    kTyreRearRightValid = 1U << 3,
    kBoardVoltageValid = 1U << 4,
    kBoardVoltageBoundsValid = 1U << 5,
    kBoardVoltageHistoryValid = 1U << 6,
    kBoardVoltageLowWarning = 1U << 7,
    kBoardVoltageLowAlarm = 1U << 8,
    kBoardVoltageHighWarning = 1U << 9,
    kBoardVoltageHighAlarm = 1U << 10,
};

constexpr uint16_t kGelLowAlarmCentiV = 1180;
constexpr uint16_t kGelLowWarningCentiV = 1220;
constexpr uint16_t kGelHighWarningCentiV = 1420;
constexpr uint16_t kGelHighAlarmCentiV = 1440;

// Temporary simulated tyre profile. Pressure is transported as whole kPa.
// Boundary ownership is deliberate: 150 kPa is amber, 180 kPa is normal,
// 200 kPa is high-warning and 250 kPa is still high-warning.
constexpr uint16_t kTyreLowAlarmKpa = 150;
constexpr uint16_t kTyreLowWarningKpa = 180;
constexpr uint16_t kTyreHighWarningKpa = 200;
constexpr uint16_t kTyreHighAlarmKpa = 250;

constexpr size_t kBoardVoltageHistorySamples = 12;

enum JourneyAltitudeFlags : uint8_t {
    kJourneyAltitudeValid = 1U << 0,
    kJourneyElevationGainValid = 1U << 1,
};

enum ChargeSummaryFlags : uint8_t {
    kChargeSummaryValid = 1U << 0,
    kChargeSummarySocValid = 1U << 1,
    kChargeSummaryEnergyValid = 1U << 2,
    kChargeSummaryDurationValid = 1U << 3,
};

enum PresentationLanguage : uint8_t {
    kLanguageGerman = 0,
    kLanguageEnglish = 1,
    kLanguageFrench = 2,
    kLanguageItalian = 3,
};

enum PresentationUnitSystem : uint8_t {
    kUnitsMetric = 0,
    kUnitsUk = 1,
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

// Persistent presentation preference shared between one adapter and its bonded
// display. Values remain presentation-only; vehicle data stays canonical metric.
struct PresentationConfigV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t language;
    uint8_t unitSystem;
    uint8_t reserved;
    uint16_t revision;
    uint16_t crc16;
};

// Additive, adapter-owned snapshot of the last stably completed charge. It is
// independent from the live ExtendedTelemetryV1 charging fields and survives
// display replacement or restart.
struct ChargeSummaryV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t sequence;
    uint8_t startSocPercent;
    uint8_t endSocPercent;
    uint16_t durationMinutes;
    uint32_t energyChargedWh;
    uint16_t crc16;
};

// Additive live journey elevation record. Altitude is the adapter's accepted
// moving GNSS value; ascent/descent use bounded 30-second samples.
struct JourneyAltitudeV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t sequence;
    int16_t altitudeMeters;
    uint16_t ascentMeters;
    uint16_t descentMeters;
    uint16_t sourceAgeMs;
    uint16_t crc16;
};

// Optional board-network snapshot. Tyre pressure is canonical whole kPa and
// voltage is canonical centivolts. History is oldest to newest and deliberately
// bounded for display use; unused entries are zero.
struct VehicleBoardNetworkV1 {
    uint16_t magic;
    uint8_t version;
    uint16_t flags;
    uint16_t sequence;
    uint16_t tyrePressureKpa[4];  // front-left, front-right, rear-left, rear-right
    uint16_t voltageCentiV;
    uint16_t minimumCentiV;
    uint16_t maximumCentiV;
    uint8_t historyCount;
    uint8_t historyCadenceMinutes;
    uint16_t historyCentiV[kBoardVoltageHistorySamples];
    uint16_t sourceAgeSeconds;
    uint16_t crc16;
};

enum SoftwareVersionFlags : uint8_t {
    kVcuSoftwareVersionValid = 1U << 0,
    kIpcSoftwareVersionValid = 1U << 1,
    kBduSoftwareVersionValid = 1U << 2,
    kIpcSoftwareVersionCandidate = 1U << 3,
    kBduSoftwareVersionCandidate = 1U << 4,
};

struct SoftwareVersionsV1 {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t sequence;
    uint8_t vcuMajor;
    uint8_t vcuMinor;
    uint16_t vcuBuild;
    uint8_t ipcMajor;
    uint8_t ipcMinor;
    uint8_t ipcPatch;
    uint8_t bduMajor;
    uint8_t bduMinor;
    uint8_t bduPatch;
    uint16_t crc16;
};
#pragma pack(pop)

static_assert(sizeof(TelemetryV1) == 16, "BLE display packet must remain 16 bytes");
static_assert(sizeof(StatusV1) == 60, "BLE display status packet must remain 60 bytes");
static_assert(sizeof(CapabilitiesV1) == 10, "BLE capability packet must remain 10 bytes");
static_assert(sizeof(ExtendedTelemetryV1) == 32, "BLE extended packet must remain 32 bytes");
static_assert(sizeof(DrivingStatusV1) == 14, "BLE driving packet must be 14 bytes");
static_assert(sizeof(PresentationConfigV1) == 10, "BLE presentation config must be 10 bytes");
static_assert(sizeof(ChargeSummaryV1) == 16, "BLE charge summary must be 16 bytes");
static_assert(sizeof(JourneyAltitudeV1) == 16, "BLE journey altitude must be 16 bytes");
static_assert(sizeof(VehicleBoardNetworkV1) == 51,
              "BLE vehicle/board-network packet must remain 51 bytes");
static_assert(sizeof(SoftwareVersionsV1) == 18,
              "BLE software-version packet must remain 18 bytes");

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

inline void seal(PresentationConfigV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(PresentationConfigV1, crc16));
}

inline bool valid(const PresentationConfigV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.language <= kLanguageItalian && packet.unitSystem <= kUnitsUk &&
        packet.reserved == 0 &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(PresentationConfigV1, crc16));
}

inline void seal(SoftwareVersionsV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(SoftwareVersionsV1, crc16));
}

inline bool valid(const SoftwareVersionsV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(SoftwareVersionsV1, crc16));
}

inline void seal(ChargeSummaryV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(ChargeSummaryV1, crc16));
}

inline bool valid(const ChargeSummaryV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        (!(packet.flags & kChargeSummarySocValid) ||
         (packet.startSocPercent <= 100 && packet.endSocPercent <= 100)) &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(ChargeSummaryV1, crc16));
}

inline void seal(JourneyAltitudeV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(JourneyAltitudeV1, crc16));
}

inline bool valid(const JourneyAltitudeV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(JourneyAltitudeV1, crc16));
}

inline void seal(VehicleBoardNetworkV1 &packet)
{
    packet.crc16 = crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                             offsetof(VehicleBoardNetworkV1, crc16));
}

inline bool valid(const VehicleBoardNetworkV1 &packet)
{
    return packet.magic == kMagic && packet.version == kProtocolVersion &&
        packet.historyCount <= kBoardVoltageHistorySamples &&
        packet.crc16 == crc16Ccitt(reinterpret_cast<const uint8_t *>(&packet),
                                  offsetof(VehicleBoardNetworkV1, crc16));
}

}  // namespace MotDisplayBle
