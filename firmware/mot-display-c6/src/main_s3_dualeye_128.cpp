#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <SPI.h>
#include <cmath>

#include "dualeye_fun_assets.h"
#include "mot_ble_client.h"

namespace {

constexpr char kFirmwareVersion[] = "MOT-DISPLAY-DUALEYE-1.28-0.1-PRE9";
constexpr int kSize = 240;
constexpr int kCenter = 120;
constexpr uint32_t kFrameMs = 250;
constexpr uint32_t kInputDebounceMs = 250;
constexpr uint32_t kButtonDebounceMs = 35;
constexpr uint32_t kPairingResetHoldMs = 3000;
constexpr uint32_t kTouchReleaseMs = 350;

constexpr int kLcdSck = 41;
constexpr int kLcdMosi = 42;
constexpr int kLcdMiso = 40;
constexpr int kLcdDc = 45;
constexpr int kLeftCs = 47;
constexpr int kLeftReset = 48;
constexpr int kLeftBacklight = 46;
constexpr int kRightCs = 38;
constexpr int kRightReset = 8;
constexpr int kRightBacklight = 39;

constexpr int kRightTouchInterrupt = 7;
constexpr int kRightTouchReset = 6;
constexpr int kRightTouchSda = 3;
constexpr int kRightTouchScl = 2;
constexpr int kLeftTouchInterrupt = 5;
constexpr int kLeftTouchReset = 4;
constexpr int kLeftTouchSda = 11;
constexpr int kLeftTouchScl = 10;
constexpr int kPageButton = 0;
constexpr uint32_t kOverspeedDelayMs = 1500;
constexpr uint32_t kWarningBlinkMs = 500;
constexpr uint32_t kFunScreenMs = 11000;

constexpr uint16_t kBackground = RGB565_BLACK;
constexpr uint16_t kForeground = RGB565_WHITE;
constexpr uint16_t kMuted = 0x7BEF;
constexpr uint16_t kTrack = 0x18E3;
constexpr uint16_t kConsumption = 0xFBE0;
constexpr uint16_t kRegen = 0x07F3;
constexpr uint16_t kCharging = 0x051F;
constexpr uint16_t kWarning = 0xFFE0;
constexpr uint16_t kAlarm = 0xF800;

SPIClass displaySpi(FSPI);
Arduino_DataBus *leftBus = new Arduino_HWSPI(
    kLcdDc, kLeftCs, kLcdSck, kLcdMosi, kLcdMiso, &displaySpi, true);
Arduino_DataBus *rightBus = new Arduino_HWSPI(
    kLcdDc, kRightCs, kLcdSck, kLcdMosi, kLcdMiso, &displaySpi, true);
Arduino_GFX *leftPanel = new Arduino_GC9A01(leftBus, kLeftReset, 1, true);
Arduino_GFX *rightPanel = new Arduino_GC9A01(rightBus, kRightReset, 3, true);
Arduino_Canvas *leftCanvas = new Arduino_Canvas(kSize, kSize, leftPanel);
Arduino_Canvas *rightCanvas = new Arduino_Canvas(kSize, kSize, rightPanel);
enum class RightPage : uint8_t {
  Journey, Battery, Charging, VehicleBoard, SoftwareVersions, Status, SpeedControl, Fun
};
RightPage rightPage = RightPage::Journey;
uint32_t lastFrameMs = 0;
uint32_t lastTouchMs = 0;
volatile bool rightTouchEvent = false;
volatile bool leftTouchEvent = false;
uint8_t speedLimitValue = 0;
uint32_t overspeedStartedMs = 0;
uint32_t lastWarningBlinkMs = 0;
bool warningRingVisible = false;
uint32_t funScreenUntilMs = 0;
uint32_t funScreenTouchEnabledMs = 0;
bool buttonWasPressed = false;
uint32_t buttonChangedMs = 0;
uint32_t buttonPressedMs = 0;
uint32_t pairingResetMessageUntilMs = 0;
bool buttonLongActionHandled = false;
bool touchContactActive = false;
bool touchLongActionHandled = false;
bool touchStartedOnRight = false;
uint16_t touchStartX = 0;
uint16_t touchStartY = 0;
uint32_t touchContactStartedMs = 0;
uint32_t touchContactLastSeenMs = 0;
RightPage touchStartPage = RightPage::Journey;
Preferences uiPreferences;

void printCentered(Arduino_GFX *gfx, const String &text, int16_t y, uint8_t size,
                   uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color, kBackground);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  gfx->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  gfx->setCursor((kSize - static_cast<int>(width)) / 2, y);
  gfx->print(text);
}

void printCenteredInHalf(Arduino_GFX *gfx, const String &text, bool rightHalf,
                         int16_t y, uint8_t size, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color, kBackground);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  gfx->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  const int16_t origin = rightHalf ? kSize / 2 : 0;
  gfx->setCursor(origin + (kSize / 2 - static_cast<int>(width)) / 2, y);
  gfx->print(text);
}

String fixedTenths(int32_t value, bool plus = false) {
  const bool negative = value < 0;
  const uint32_t magnitude = static_cast<uint32_t>(negative ? -value : value);
  String result = negative ? "-" : (plus ? "+" : "");
  result += String(magnitude / 10U);
  result += ".";
  result += String(magnitude % 10U);
  return result;
}

String durationText(uint16_t minutes) {
  if (minutes < 60) return String(minutes) + " min";
  return String(minutes / 60) + "h " + String(minutes % 60) + "min";
}

void drawArc(Arduino_GFX *gfx, float value, float maximum, uint16_t color) {
  constexpr int segments = 45;
  const int active = static_cast<int>(roundf(
      constrain(value, 0.0f, maximum) / maximum * segments));
  for (int index = 0; index < segments; ++index) {
    const float degrees = 135.0f + 270.0f * index / (segments - 1);
    const float radians = degrees * PI / 180.0f;
    const int16_t xo = kCenter + static_cast<int16_t>(cosf(radians) * 113.0f);
    const int16_t yo = kCenter + static_cast<int16_t>(sinf(radians) * 113.0f);
    const int16_t xi = kCenter + static_cast<int16_t>(cosf(radians) * 104.0f);
    const int16_t yi = kCenter + static_cast<int16_t>(sinf(radians) * 104.0f);
    gfx->drawLine(xi, yi, xo, yo, index < active ? color : kTrack);
    gfx->drawLine(xi + 1, yi, xo + 1, yo, index < active ? color : kTrack);
  }
}

void updateSpeedWarning(bool speedValid, const MotDisplayBle::DrivingStatusV1 &driving) {
  if (!speedValid || speedLimitValue == 0) {
    overspeedStartedMs = 0;
    warningRingVisible = false;
    return;
  }
  const float speed = driving.speedDeciKmh / 10.0f;
  const float tolerance = speedLimitValue == 30 ? 2.0f : 3.0f;
  if (speed <= speedLimitValue + tolerance) {
    overspeedStartedMs = 0;
    warningRingVisible = false;
    return;
  }
  const uint32_t now = millis();
  if (!overspeedStartedMs) overspeedStartedMs = now;
  if (now - overspeedStartedMs < kOverspeedDelayMs) return;
  if (now - lastWarningBlinkMs >= kWarningBlinkMs) {
    lastWarningBlinkMs = now;
    warningRingVisible = !warningRingVisible;
  }
}

bool latestTelemetry(MotDisplayBle::TelemetryV1 &packet) {
  uint32_t age = 0;
  return motBleClientLatest(packet, age);
}

bool latestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet) {
  uint32_t age = 0;
  return motBleClientLatestExtended(packet, age);
}

void renderLeft() {
  if (rightPage == RightPage::Fun) {
    leftCanvas->draw16bitRGBBitmap(0, 0,
        const_cast<uint16_t *>(kDualEyeMicrolinoPhoto), kSize, kSize);
    leftCanvas->flush();
    return;
  }
  MotDisplayBle::TelemetryV1 telemetry{};
  MotDisplayBle::DrivingStatusV1 driving{};
  MotDisplayBle::ExtendedTelemetryV1 extended{};
  uint32_t drivingAge = 0;
  const bool telemetryValid = latestTelemetry(telemetry);
  const bool drivingValid = motBleClientLatestDriving(driving, drivingAge);
  const bool extendedValid = latestExtended(extended);
  const bool charging = telemetryValid && (telemetry.flags & MotDisplayBle::kCharging);
  const bool socValid = telemetryValid && (telemetry.flags & MotDisplayBle::kSocValid);
  const uint8_t soc = socValid ? min<uint8_t>(telemetry.socPercent, 100) : 0;

  leftCanvas->fillScreen(kBackground);
  if (charging) {
    drawArc(leftCanvas, soc, 100.0f, soc >= 80 ? kRegen : soc < 20 ? kAlarm : kWarning);
    printCentered(leftCanvas, "LADEN", 29, 2, kCharging);
    printCentered(leftCanvas, socValid ? "SOC " + String(soc) + "%" : "SOC --%",
                  62, 2, kForeground);
    const bool power = telemetry.flags & MotDisplayBle::kDataValid;
    printCentered(leftCanvas, power ? fixedTenths(abs(telemetry.powerDeciKw)) + " kW" : "-- kW",
                  104, 3, kRegen);
    const bool energy = extendedValid &&
        (extended.flags & MotDisplayBle::kChargeEnergyValid);
    printCentered(leftCanvas, energy ? "GELADEN " +
        String(extended.chargeEnergyWh / 1000.0f, 2) + " kWh" : "GELADEN --",
        158, 1, energy ? kForeground : kMuted);
    printCentered(leftCanvas, "MOT DISPLAY", 202, 1, kMuted);
    leftCanvas->flush();
    return;
  }

  const bool powerValid = telemetryValid && (telemetry.flags & MotDisplayBle::kDataValid);
  const int16_t power = powerValid ? telemetry.powerDeciKw : 0;
  const bool regen = telemetryValid && (telemetry.flags & MotDisplayBle::kRegenerating);
  const uint16_t flowColor = regen ? kRegen : power > 0 ? kConsumption : kMuted;
  drawArc(leftCanvas, fabsf(power / 10.0f), 25.0f, flowColor);
  printCentered(leftCanvas, socValid ? "SOC " + String(soc) + "%" : "SOC --%",
                25, 2, socValid ? kForeground : kMuted);

  const bool indicator = drivingValid && (driving.flags & MotDisplayBle::kIndicatorStateValid);
  if (indicator && (driving.flags & MotDisplayBle::kLeftIndicatorActive)) {
    leftCanvas->fillTriangle(20, 47, 42, 34, 42, 60, kRegen);
  }
  if (indicator && (driving.flags & MotDisplayBle::kRightIndicatorActive)) {
    leftCanvas->fillTriangle(220, 47, 198, 34, 198, 60, kRegen);
  }
  const bool speed = drivingValid && (driving.flags & MotDisplayBle::kDrivingSpeedValid);
  updateSpeedWarning(speed, driving);
  printCentered(leftCanvas, speed ? String((driving.speedDeciKmh + 5) / 10) : "--", 67, 5,
                speed ? kForeground : kMuted);
  printCentered(leftCanvas, "km/h", 112, 2, kMuted);
  printCentered(leftCanvas, powerValid ? fixedTenths(power, true) + " kW" : "--.- kW",
                145, 2, flowColor);
  const bool range = telemetryValid && (telemetry.flags & MotDisplayBle::kRangeValid);
  printCentered(leftCanvas, range ? String(telemetry.rangeToTargetKm) + " km bis " +
      String(telemetry.targetSocPercent) + "%" : "-- km bis --%", 177, 1,
      range ? kForeground : kMuted);
  String footer = "BEREIT";
  if (drivingValid && (driving.flags & MotDisplayBle::kCandidateStatesValid)) {
    const uint8_t gear = static_cast<uint8_t>(
        (driving.flags & MotDisplayBle::kGearStateMask) >> MotDisplayBle::kGearStateShift);
    if (gear != MotDisplayBle::kGearUnknown)
      footer = gear == MotDisplayBle::kGearDrive ? "D" :
          gear == MotDisplayBle::kGearReverse ? "R" : "N";
    const uint8_t mode = static_cast<uint8_t>(
        (driving.flags & MotDisplayBle::kDriveModeMask) >> MotDisplayBle::kDriveModeShift);
    if (mode == MotDisplayBle::kDriveModeSport) footer += "  SPORT";
    else if (mode == MotDisplayBle::kDriveModeEco) footer += "  ECO";
  }
  printCentered(leftCanvas, footer, 204, 2, kForeground);
  if (speedLimitValue != 0)
    printCentered(leftCanvas, "LIMIT " + String(speedLimitValue), 224, 1, kMuted);
  if (warningRingVisible) {
    const bool severe = driving.speedDeciKmh / 10.0f >= speedLimitValue + 10.0f;
    leftCanvas->drawCircle(kCenter, kCenter, 118, severe ? kAlarm : kConsumption);
    leftCanvas->drawCircle(kCenter, kCenter, 117, severe ? kAlarm : kConsumption);
  }
  leftCanvas->flush();
}

void renderJourney() {
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  MotDisplayBle::JourneyAltitudeV1 altitude{};
  uint32_t altitudeAge = 0;
  const bool valid = latestExtended(packet);
  const bool journey = valid && (packet.flags & MotDisplayBle::kJourneyValid);
  rightCanvas->drawCircle(kCenter, kCenter, 113, journey ? kConsumption : kTrack);
  printCentered(rightCanvas, "AKTUELLE FAHRT", 27, 1, kForeground);
  printCentered(rightCanvas, journey ? fixedTenths(packet.journeyDistanceDeciKm) + " km" : "-- km",
                50, 3, journey ? kForeground : kMuted);
  printCentered(rightCanvas, journey ? durationText(packet.journeyDurationMinutes) : "-- min",
                88, 2, journey ? kForeground : kMuted);
  const bool consumption = journey && (packet.flags & MotDisplayBle::kJourneyConsumptionValid);
  printCentered(rightCanvas, consumption ?
      fixedTenths(packet.journeyConsumptionDeciKwhPer100Km) + " kWh/100" : "-- kWh/100",
      120, 2, consumption ? kConsumption : kMuted);
  const bool peak = journey && (packet.flags & MotDisplayBle::kRegenPeakValid);
  printCentered(rightCanvas, peak ? "REKU " + fixedTenths(packet.regenPeakDeciKw) + " kW" :
      "REKU --", 153, 1, peak ? kRegen : kMuted);
  const bool altitudeValid = motBleClientLatestJourneyAltitude(altitude, altitudeAge) &&
      (altitude.flags & MotDisplayBle::kJourneyAltitudeValid) && altitude.sourceAgeMs <= 5000;
  String altitudeText = altitudeValid ? "HOEHE " + String(altitude.altitudeMeters) + " m" : "";
  if (altitudeValid && (altitude.flags & MotDisplayBle::kJourneyElevationGainValid))
    altitudeText += "  +" + String(altitude.ascentMeters) + "/-" +
        String(altitude.descentMeters) + " m";
  printCentered(rightCanvas, altitudeText, 174, 1, kMuted);
}

void renderBattery() {
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  const bool valid = latestExtended(packet);
  const bool battery = valid && (packet.flags & MotDisplayBle::kBatteryValuesValid);
  printCentered(rightCanvas, "TRAKTIONSBATTERIE", 28, 1, kForeground);
  printCentered(rightCanvas, valid && (packet.flags & MotDisplayBle::kSohValid) ?
      "SOH " + String(packet.sohHundredths / 100.0f, 1) + "%" : "SOH --", 57, 2,
      valid ? kForeground : kMuted);
  printCentered(rightCanvas, battery ? fixedTenths(packet.packVoltageDeciV) + " V" : "-- V",
                94, 3, battery ? kForeground : kMuted);
  printCentered(rightCanvas, battery ? fixedTenths(packet.packCurrentDeciA, true) + " A" : "-- A",
                136, 2, battery ? kForeground : kMuted);
  const bool delta = valid && (packet.flags & MotDisplayBle::kCellDeltaValid);
  printCentered(rightCanvas, delta ? "DELTA " + String(packet.cellDeltaMv) + " mV" : "DELTA --",
                174, 2, delta ? kWarning : kMuted);
}

void renderCharging() {
  MotDisplayBle::TelemetryV1 telemetry{};
  MotDisplayBle::ExtendedTelemetryV1 extended{};
  MotDisplayBle::ChargeSummaryV1 summary{};
  uint32_t summaryAge = 0;
  const bool live = latestTelemetry(telemetry) && (telemetry.flags & MotDisplayBle::kCharging);
  const bool extendedValid = latestExtended(extended);
  const bool summaryValid = motBleClientLatestChargeSummary(summary, summaryAge) &&
      (summary.flags & MotDisplayBle::kChargeSummaryValid);
  printCentered(rightCanvas, live ? "LADEN" : "LETZTE LADUNG", 27, 2,
                live ? kCharging : kForeground);
  if (live) {
    const bool soc = telemetry.flags & MotDisplayBle::kSocValid;
    printCentered(rightCanvas, soc ? "SOC " + String(telemetry.socPercent) + "%" : "SOC --%",
                  62, 3, soc ? kForeground : kMuted);
    const bool power = telemetry.flags & MotDisplayBle::kDataValid;
    printCentered(rightCanvas, power ? fixedTenths(abs(telemetry.powerDeciKw)) + " kW" : "-- kW",
                  108, 3, power ? kRegen : kMuted);
    const bool energy = extendedValid && (extended.flags & MotDisplayBle::kChargeEnergyValid);
    printCentered(rightCanvas, energy ? String(extended.chargeEnergyWh / 1000.0f, 2) + " kWh" :
                  "-- kWh", 160, 2, energy ? kForeground : kMuted);
  } else {
    const bool soc = summaryValid && (summary.flags & MotDisplayBle::kChargeSummarySocValid);
    printCentered(rightCanvas, soc ? "SOC " + String(summary.startSocPercent) + " -> " +
        String(summary.endSocPercent) + "%" : "SOC --", 62, 2, soc ? kForeground : kMuted);
    const bool energy = summaryValid && (summary.flags & MotDisplayBle::kChargeSummaryEnergyValid);
    printCentered(rightCanvas, energy ? String(summary.energyChargedWh / 1000.0f, 2) + " kWh" :
                  "-- kWh", 105, 3, energy ? kRegen : kMuted);
    const bool duration = summaryValid && (summary.flags & MotDisplayBle::kChargeSummaryDurationValid);
    printCentered(rightCanvas, duration ? durationText(summary.durationMinutes) : "-- min",
                  157, 2, duration ? kForeground : kMuted);
  }
}

String tyrePressure(const MotDisplayBle::VehicleBoardNetworkV1 &packet,
                    uint16_t flag, uint8_t index) {
  if (!(packet.flags & flag)) return "--";
  return String(packet.tyrePressureKpa[index] / 100.0f, 2);
}

uint16_t tyrePressureColor(const MotDisplayBle::VehicleBoardNetworkV1 &packet,
                           uint16_t flag, uint8_t index) {
  if (!(packet.flags & flag)) return kMuted;
  const uint16_t value = packet.tyrePressureKpa[index];
  if (value < MotDisplayBle::kTyreLowAlarmKpa || value > MotDisplayBle::kTyreHighAlarmKpa)
    return kAlarm;
  if (value < MotDisplayBle::kTyreLowWarningKpa || value >= MotDisplayBle::kTyreHighWarningKpa)
    return kWarning;
  return kRegen;
}

void renderVehicleBoard() {
  MotDisplayBle::VehicleBoardNetworkV1 packet{};
  uint32_t age = 0;
  const bool fresh = motBleClientLatestVehicleBoardNetwork(packet, age);
  printCentered(rightCanvas, "FAHRZEUG & 12 V", 24, 1, kForeground);
  const String frontLeft = "VL " + (fresh ? tyrePressure(packet,
      MotDisplayBle::kTyreFrontLeftValid, 0) : "--");
  const String frontRight = "VR " + (fresh ? tyrePressure(packet,
      MotDisplayBle::kTyreFrontRightValid, 1) : "--");
  const String rearLeft = "HL " + (fresh ? tyrePressure(packet,
      MotDisplayBle::kTyreRearLeftValid, 2) : "--");
  const String rearRight = "HR " + (fresh ? tyrePressure(packet,
      MotDisplayBle::kTyreRearRightValid, 3) : "--");
  printCenteredInHalf(rightCanvas, frontLeft, false, 48, 1, fresh ? tyrePressureColor(
      packet, MotDisplayBle::kTyreFrontLeftValid, 0) : kMuted);
  printCenteredInHalf(rightCanvas, frontRight, true, 48, 1, fresh ? tyrePressureColor(
      packet, MotDisplayBle::kTyreFrontRightValid, 1) : kMuted);
  printCenteredInHalf(rightCanvas, rearLeft, false, 66, 1, fresh ? tyrePressureColor(
      packet, MotDisplayBle::kTyreRearLeftValid, 2) : kMuted);
  printCenteredInHalf(rightCanvas, rearRight, true, 66, 1, fresh ? tyrePressureColor(
      packet, MotDisplayBle::kTyreRearRightValid, 3) : kMuted);
  printCentered(rightCanvas, "REIFENDRUCK bar", 84, 1, kMuted);

  const bool voltage = fresh && (packet.flags & MotDisplayBle::kBoardVoltageValid);
  const bool bounds = fresh && (packet.flags & MotDisplayBle::kBoardVoltageBoundsValid);
  const bool alarm = voltage && (packet.flags &
      (MotDisplayBle::kBoardVoltageLowAlarm | MotDisplayBle::kBoardVoltageHighAlarm));
  const bool warning = voltage && (packet.flags &
      (MotDisplayBle::kBoardVoltageLowWarning | MotDisplayBle::kBoardVoltageHighWarning));
  const uint16_t voltageColor = alarm ? kAlarm : warning ? kWarning : voltage ? kRegen : kMuted;
  printCentered(rightCanvas, voltage ? String(packet.voltageCentiV / 100.0f, 2) + " V" : "-- V",
                105, 3, voltageColor);
  const String state = alarm ? "ALARM" : warning ? "GEL PRUEFEN" : voltage ? "GEL OK" : "STATUS --";
  printCentered(rightCanvas, state, 133, 1, voltageColor);
  printCentered(rightCanvas, bounds ? "MIN " + String(packet.minimumCentiV / 100.0f, 2) +
      "  MAX " + String(packet.maximumCentiV / 100.0f, 2) : "MIN --  MAX --",
      146, 1, bounds ? kForeground : kMuted);

  constexpr int x = 35, y = 163, width = 170, height = 39;
  rightCanvas->drawRect(x, y, width, height, kTrack);
  const bool history = fresh && (packet.flags & MotDisplayBle::kBoardVoltageHistoryValid) &&
      packet.historyCount > 1 && packet.historyCount <= MotDisplayBle::kBoardVoltageHistorySamples;
  if (!history) {
    printCentered(rightCanvas, "KEIN 12-V-VERLAUF", 176, 1, kMuted);
    return;
  }
  uint16_t minimum = packet.historyCentiV[0];
  uint16_t maximum = minimum;
  for (uint8_t i = 1; i < packet.historyCount; ++i) {
    minimum = min(minimum, packet.historyCentiV[i]);
    maximum = max(maximum, packet.historyCentiV[i]);
  }
  const uint16_t span = max<uint16_t>(maximum - minimum, 10U);
  for (uint8_t i = 1; i < packet.historyCount; ++i) {
    const int x0 = x + 3 + (i - 1) * (width - 7) / (packet.historyCount - 1);
    const int x1 = x + 3 + i * (width - 7) / (packet.historyCount - 1);
    const int y0 = y + height - 4 -
        (packet.historyCentiV[i - 1] - minimum) * (height - 8) / span;
    const int y1 = y + height - 4 -
        (packet.historyCentiV[i] - minimum) * (height - 8) / span;
    rightCanvas->drawLine(x0, y0, x1, y1, kCharging);
  }
}

void renderStatus() {
  if (static_cast<int32_t>(pairingResetMessageUntilMs - millis()) > 0) {
    printCentered(rightCanvas, "NEU KOPPELN", 75, 2, kWarning);
    printCentered(rightCanvas, "SUCHE MOT", 112, 2, kForeground);
    printCentered(rightCanvas, "FENSTER 10 MIN", 151, 1, kMuted);
    return;
  }
  MotDisplayBle::StatusV1 status{};
  uint32_t age = 0;
  const bool valid = motBleClientLatestStatus(status, age);
  printCentered(rightCanvas, "MOT STATUS", 27, 2, kForeground);
  if (!valid) {
    const String bleStatus = motBleClientStatus();
    printCentered(rightCanvas, bleStatus, 105, bleStatus.length() > 16 ? 1 : 2, kMuted);
    return;
  }
  const bool wifi1 = status.flags & MotDisplayBle::kWifi1Active;
  const bool wifi2 = status.flags & MotDisplayBle::kWifi2Active;
  printCentered(rightCanvas, "WIFI1 " + String(status.wifi1Name), 62, 1,
                wifi1 ? kRegen : kMuted);
  printCentered(rightCanvas, "WIFI2 " + String(status.wifi2Name), 84, 1,
                wifi2 ? kRegen : kMuted);
  printCentered(rightCanvas, String("IP ") + status.ipAddress, 115, 1,
                (status.flags & MotDisplayBle::kNetworkOnline) ? kForeground : kMuted);
  printCentered(rightCanvas, status.flags2 & MotDisplayBle::kAccessPointActive ? "AP AKTIV" : "AP AUS",
                140, 1, (status.flags2 & MotDisplayBle::kAccessPointActive) ? kWarning : kMuted);
  const bool aws = status.flags & MotDisplayBle::kAwsConnected;
  printCentered(rightCanvas, aws ? "AWS VERBUNDEN" : "AWS GETRENNT", 171, 2,
                aws ? kRegen : kMuted);
  printCentered(rightCanvas, "3S TOUCH/TASTE: KOPPELN", 198, 1, kMuted);
}

void renderSoftwareVersions() {
  MotDisplayBle::SoftwareVersionsV1 packet{};
  uint32_t age = 0;
  const bool valid = motBleClientLatestSoftwareVersions(packet, age);
  printCentered(rightCanvas, "SOFTWARE-VERSIONEN", 27, 1, kForeground);
  const bool vcu = valid && (packet.flags & MotDisplayBle::kVcuSoftwareVersionValid);
  const bool ipc = valid && (packet.flags & MotDisplayBle::kIpcSoftwareVersionValid);
  const bool bdu = valid && (packet.flags & MotDisplayBle::kBduSoftwareVersionValid);
  const auto version = [](uint8_t major, uint8_t minor, uint16_t patch) {
    return String(major) + "." + String(minor) + "." + String(patch);
  };
  printCentered(rightCanvas, vcu ? "VCU  " + version(packet.vcuMajor, packet.vcuMinor, packet.vcuBuild)
                                 : "VCU  --", 67, 2, vcu ? kRegen : kMuted);
  printCentered(rightCanvas, ipc ? "IPC  " + version(packet.ipcMajor, packet.ipcMinor, packet.ipcPatch)
                                 : "IPC  --", 109, 2, ipc ? kForeground : kMuted);
  printCentered(rightCanvas, bdu ? "BDU  " + version(packet.bduMajor, packet.bduMinor, packet.bduPatch)
                                 : "BDU  --", 151, 2, bdu ? kForeground : kMuted);
  printCentered(rightCanvas, "PASSIV EMPFANGEN", 190, 1, kMuted);
}

void renderSpeedControl() {
  printCentered(rightCanvas, "TEMPOLIMIT", 27, 2, kForeground);
  struct Choice { int16_t x; int16_t y; uint8_t value; const char *label; };
  constexpr Choice choices[] = {
      {28, 58, 30, "30"}, {124, 58, 50, "50"},
      {28, 112, 60, "60"}, {124, 112, 80, "80"},
      {28, 166, 0, "AUS"}, {124, 166, 255, "WEITER"},
  };
  for (const Choice &choice : choices) {
    const bool selected = choice.value != 255 && speedLimitValue == choice.value;
    rightCanvas->drawRoundRect(choice.x, choice.y, 88, 45, 8,
                               selected ? kConsumption : kMuted);
    rightCanvas->drawRoundRect(choice.x + 1, choice.y + 1, 86, 43, 7,
                               selected ? kConsumption : kTrack);
    rightCanvas->setTextSize(choice.value == 255 ? 1 : 2);
    rightCanvas->setTextColor(choice.value == 255 ? kRegen : kForeground, kBackground);
    int16_t x1, y1;
    uint16_t w, h;
    rightCanvas->getTextBounds(choice.label, 0, 0, &x1, &y1, &w, &h);
    rightCanvas->setCursor(choice.x + (88 - static_cast<int>(w)) / 2,
                           choice.y + (45 - static_cast<int>(h)) / 2);
    rightCanvas->print(choice.label);
  }
}

void renderRight() {
  if (rightPage == RightPage::Fun) {
    rightCanvas->draw16bitRGBBitmap(0, 0,
        const_cast<uint16_t *>(kDualEyeMotVector), kSize, kSize);
    rightCanvas->flush();
    return;
  }
  rightCanvas->fillScreen(kBackground);
  switch (rightPage) {
    case RightPage::Journey: renderJourney(); break;
    case RightPage::Battery: renderBattery(); break;
    case RightPage::Charging: renderCharging(); break;
    case RightPage::VehicleBoard: renderVehicleBoard(); break;
    case RightPage::SoftwareVersions: renderSoftwareVersions(); break;
    case RightPage::Status: renderStatus(); break;
    case RightPage::SpeedControl: renderSpeedControl(); break;
    case RightPage::Fun: break;
  }
  const uint8_t pageNumber = static_cast<uint8_t>(rightPage) + 1U;
  printCentered(rightCanvas, "TOUCH  " + String(pageNumber) + "/8", 217, 1, kMuted);
  rightCanvas->flush();
}

void ARDUINO_ISR_ATTR onRightTouch() {
  rightTouchEvent = true;
}

void ARDUINO_ISR_ATTR onLeftTouch() {
  leftTouchEvent = true;
}

struct TouchBus {
  int sda;
  int scl;
};

constexpr TouchBus kRightTouchBus{kRightTouchSda, kRightTouchScl};
constexpr TouchBus kLeftTouchBus{kLeftTouchSda, kLeftTouchScl};

void touchDelay() { delayMicroseconds(4); }
void touchSda(const TouchBus &bus, bool high) {
  pinMode(bus.sda, high ? INPUT_PULLUP : OUTPUT);
  if (!high) digitalWrite(bus.sda, LOW);
}
void touchScl(const TouchBus &bus, bool high) {
  pinMode(bus.scl, high ? INPUT_PULLUP : OUTPUT);
  if (!high) digitalWrite(bus.scl, LOW);
  touchDelay();
}
void touchStart(const TouchBus &bus) {
  touchSda(bus, true); touchScl(bus, true); touchSda(bus, false);
  touchDelay(); touchScl(bus, false);
}
void touchStop(const TouchBus &bus) {
  touchSda(bus, false); touchScl(bus, true); touchSda(bus, true); touchDelay();
}
bool touchWriteByte(const TouchBus &bus, uint8_t value) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    touchSda(bus, value & mask); touchScl(bus, true); touchScl(bus, false);
  }
  touchSda(bus, true); touchScl(bus, true);
  const bool ack = digitalRead(bus.sda) == LOW;
  touchScl(bus, false);
  return ack;
}
uint8_t touchReadByte(const TouchBus &bus, bool ack) {
  uint8_t value = 0; touchSda(bus, true);
  for (int bit = 0; bit < 8; ++bit) {
    value <<= 1; touchScl(bus, true);
    if (digitalRead(bus.sda)) value |= 1;
    touchScl(bus, false);
  }
  touchSda(bus, !ack); touchScl(bus, true); touchScl(bus, false);
  touchSda(bus, true);
  return value;
}
bool readTouchPosition(const TouchBus &bus, uint16_t &x, uint16_t &y) {
  uint8_t data[5]{};
  touchStart(bus);
  if (!touchWriteByte(bus, 0x15 << 1) || !touchWriteByte(bus, 0x02)) {
    touchStop(bus);
    return false;
  }
  touchStop(bus); touchStart(bus);
  if (!touchWriteByte(bus, (0x15 << 1) | 1)) { touchStop(bus); return false; }
  for (int i = 0; i < 5; ++i) data[i] = touchReadByte(bus, i < 4);
  touchStop(bus);
  if ((data[0] & 0x0F) == 0) return false;
  const uint16_t rawX = ((data[1] & 0x0F) << 8) | data[2];
  const uint16_t rawY = ((data[3] & 0x0F) << 8) | data[4];
  x = constrain(239 - static_cast<int>(rawY), 0, 239);
  y = constrain(static_cast<int>(rawX), 0, 239);
  return true;
}

void handleSpeedControlTouch(uint16_t x, uint16_t y) {
  if (y >= 58 && y < 103) speedLimitValue = x < 120 ? 30 : 50;
  else if (y >= 112 && y < 157) speedLimitValue = x < 120 ? 60 : 80;
  else if (y >= 166 && y < 211) {
    if (x < 120) speedLimitValue = 0;
    else {
      rightPage = RightPage::Fun;
      const uint32_t now = millis();
      funScreenUntilMs = now + kFunScreenMs;
      funScreenTouchEnabledMs = now + 600U;
      noInterrupts();
      rightTouchEvent = false;
      leftTouchEvent = false;
      interrupts();
      return;
    }
  } else return;
  uiPreferences.putUChar("speedLimit", speedLimitValue);
  overspeedStartedMs = 0;
  warningRingVisible = false;
  Serial.printf("DualEye: speed limit=%u\n", speedLimitValue);
}

void dismissFunScreen(uint32_t now) {
  rightPage = RightPage::Journey;
  funScreenUntilMs = 0;
  funScreenTouchEnabledMs = 0;
  lastTouchMs = now;
  lastFrameMs = 0;
}

void advancePage(uint32_t now);

void beginOrContinueTouch(bool rightValid, uint16_t rightX, uint16_t rightY,
                          bool leftValid, uint32_t now) {
  if (!rightValid && !leftValid) return;
  if (!touchContactActive) {
    touchContactActive = true;
    touchLongActionHandled = false;
    touchStartedOnRight = rightValid;
    touchStartX = rightX;
    touchStartY = rightY;
    touchContactStartedMs = now;
    touchStartPage = rightPage;
  }
  touchContactLastSeenMs = now;

  if (touchStartPage == RightPage::Fun && !touchLongActionHandled &&
      static_cast<int32_t>(now - funScreenTouchEnabledMs) >= 0) {
    dismissFunScreen(now);
    touchLongActionHandled = true;
  } else if (touchStartPage == RightPage::Status && !touchLongActionHandled &&
             now - touchContactStartedMs >= kPairingResetHoldMs) {
    touchLongActionHandled = true;
    pairingResetMessageUntilMs = now + 2500U;
    motBleClientForgetPairing();
    lastFrameMs = 0;
    Serial.println("DualEye: status touch hold cleared MOT pairing");
  }
}

void finishReleasedTouch(uint32_t now) {
  if (!touchContactActive || now - touchContactLastSeenMs < kTouchReleaseMs) return;
  if (!touchLongActionHandled) {
    if (touchStartPage == RightPage::SpeedControl && touchStartedOnRight)
      handleSpeedControlTouch(touchStartX, touchStartY);
    else
      advancePage(now);
  }
  touchContactActive = false;
  touchLongActionHandled = false;
  lastTouchMs = now;
  lastFrameMs = 0;
}

void advancePage(uint32_t now) {
  // Fun is a regular final page in the right-hand page cycle. It retains the
  // bounded 11-second presentation and can still be dismissed immediately by
  // either touch surface or the GPIO0 page key.
  rightPage = static_cast<RightPage>((static_cast<uint8_t>(rightPage) + 1U) % 8U);
  if (rightPage == RightPage::Fun) {
    funScreenUntilMs = now + kFunScreenMs;
    funScreenTouchEnabledMs = now + 600U;
  }
  lastTouchMs = now;
  lastFrameMs = 0;
  Serial.printf("DualEye: right page=%u\n", static_cast<unsigned>(rightPage));
}

void pollPageButton(uint32_t now) {
  const bool pressed = digitalRead(kPageButton) == LOW;
  if (pressed != buttonWasPressed && now - buttonChangedMs >= kButtonDebounceMs) {
    buttonChangedMs = now;
    buttonWasPressed = pressed;
    if (pressed) {
      buttonPressedMs = now;
      buttonLongActionHandled = false;
    } else {
      if (!buttonLongActionHandled) {
        if (rightPage == RightPage::Fun) dismissFunScreen(now);
        else advancePage(now);
        Serial.println("DualEye: GPIO0 page button");
      }
      buttonPressedMs = 0;
      buttonLongActionHandled = false;
    }
  }
  if (pressed && !buttonLongActionHandled && rightPage == RightPage::Status &&
      buttonPressedMs && now - buttonPressedMs >= kPairingResetHoldMs) {
    buttonLongActionHandled = true;
    pairingResetMessageUntilMs = now + 2500U;
    motBleClientForgetPairing();
    lastFrameMs = 0;
    Serial.println("DualEye: GPIO0 status hold cleared MOT pairing");
  }
}

void setupTouch() {
  pinMode(kRightTouchInterrupt, INPUT_PULLUP);
  pinMode(kRightTouchReset, OUTPUT);
  digitalWrite(kRightTouchReset, LOW);
  delay(10);
  digitalWrite(kRightTouchReset, HIGH);
  delay(50);
  touchSda(kRightTouchBus, true);
  touchScl(kRightTouchBus, true);
  touchSda(kLeftTouchBus, true);
  touchScl(kLeftTouchBus, true);
  attachInterrupt(digitalPinToInterrupt(kRightTouchInterrupt), onRightTouch, FALLING);
  pinMode(kLeftTouchInterrupt, INPUT_PULLUP);
  pinMode(kLeftTouchReset, OUTPUT);
  digitalWrite(kLeftTouchReset, LOW);
  delay(10);
  digitalWrite(kLeftTouchReset, HIGH);
  delay(50);
  attachInterrupt(digitalPinToInterrupt(kLeftTouchInterrupt), onLeftTouch, FALLING);
  pinMode(kPageButton, INPUT_PULLUP);
  buttonWasPressed = digitalRead(kPageButton) == LOW;
  Serial.println("DualEye: both touch interrupts ready");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("%s boot\n", kFirmwareVersion);
  uiPreferences.begin("dualeye-ui", false);
  speedLimitValue = uiPreferences.getUChar("speedLimit", 0);
  if (speedLimitValue != 0 && speedLimitValue != 30 && speedLimitValue != 50 &&
      speedLimitValue != 60 && speedLimitValue != 80) speedLimitValue = 0;
  pinMode(kLeftBacklight, OUTPUT);
  pinMode(kRightBacklight, OUTPUT);
  digitalWrite(kLeftBacklight, LOW);
  digitalWrite(kRightBacklight, LOW);
  if (!leftCanvas->begin(80000000L) || !rightCanvas->begin(80000000L)) {
    Serial.println("DualEye: LCD or framebuffer initialization failed");
    return;
  }
  leftCanvas->fillScreen(kBackground);
  rightCanvas->fillScreen(kBackground);
  leftCanvas->flush();
  rightCanvas->flush();
  digitalWrite(kLeftBacklight, HIGH);
  digitalWrite(kRightBacklight, HIGH);
  setupTouch();
  printCentered(leftCanvas, "MOT DISPLAY", 92, 2, kForeground);
  printCentered(leftCanvas, "SUCHE MOT", 123, 2, kMuted);
  printCentered(rightCanvas, "AKTUELLE FAHRT", 92, 2, kForeground);
  printCentered(rightCanvas, "TOUCH = SEITE", 125, 1, kMuted);
  leftCanvas->flush();
  rightCanvas->flush();
  motBleClientSetup();
}

void loop() {
  motBleClientLoop();
  const uint32_t now = millis();
  pollPageButton(now);
  bool touched = false;
  bool leftTouched = false;
  noInterrupts();
  if (rightTouchEvent) {
    rightTouchEvent = false;
    touched = true;
  }
  if (leftTouchEvent) {
    leftTouchEvent = false;
    leftTouched = true;
  }
  interrupts();
  uint16_t rightX = 0, rightY = 0;
  uint16_t leftX = 0, leftY = 0;
  const bool rightTouchValid = touched &&
      readTouchPosition(kRightTouchBus, rightX, rightY);
  const bool leftTouchValid = leftTouched &&
      readTouchPosition(kLeftTouchBus, leftX, leftY);
  if (touched && !rightTouchValid) Serial.println("DualEye: right touch read failed");
  if (leftTouched && !leftTouchValid) Serial.println("DualEye: left touch read failed");
  beginOrContinueTouch(rightTouchValid, rightX, rightY, leftTouchValid, now);
  finishReleasedTouch(now);
  if (rightPage == RightPage::Fun &&
      static_cast<int32_t>(now - funScreenUntilMs) >= 0) {
    dismissFunScreen(now);
    if (touchContactActive) touchLongActionHandled = true;
  }
  if (now - lastFrameMs >= kFrameMs) {
    lastFrameMs = now;
    renderLeft();
    renderRight();
  }
  delay(5);
}
