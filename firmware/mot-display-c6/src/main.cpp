#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <cmath>

#include "mot_ble_client.h"
#include "mot_gesture.h"

namespace {

constexpr int kLcdDc = 19;
constexpr int kLcdCs = 10;
constexpr int kLcdSck = 11;
constexpr int kLcdMosi = 21;
constexpr int kLcdReset = 20;
constexpr int kLcdBacklight = 7;
constexpr char kFirmwareVersion[] = "MOT-DISPLAY-1.0.0";

constexpr int kScreenSize = 240;
constexpr int kCenter = kScreenSize / 2;
constexpr uint32_t kFrameIntervalMs = 200;
constexpr uint32_t kPairingResetNoticeMs = 2000;
constexpr uint32_t kPairingConfirmationWindowMs = 10000;
constexpr uint32_t kPageReturnMs = 15000;
constexpr uint32_t kStatusPageReturnMs = 30000;

constexpr uint16_t kBackground = RGB565_BLACK;
constexpr uint16_t kForeground = RGB565_WHITE;
constexpr uint16_t kMuted = 0x7BEF;
constexpr uint16_t kTrack = 0x18E3;
constexpr uint16_t kConsumption = 0xFBE0;
constexpr uint16_t kRegen = 0x07F3;
constexpr uint16_t kCharging = 0x051F;

Arduino_DataBus *bus = new Arduino_HWSPI(kLcdDc, kLcdCs, kLcdSck, kLcdMosi);
Arduino_GFX *panel = new Arduino_GC9A01(bus, kLcdReset, 0, true);
Arduino_Canvas *canvas = new Arduino_Canvas(kScreenSize, kScreenSize, panel);
Arduino_GFX *graphics = canvas;

enum class FlowState : uint8_t {
  Ready,
  Consumption,
  Regeneration,
  Charging,
};

struct MotDisplaySample {
  float powerKw;
  uint8_t socPercent;
  uint16_t rangeToTargetKm;
  uint8_t targetSocPercent;
  FlowState state;
  bool powerValid;
  bool socValid;
  bool rangeValid;
};

enum class DisplayPage : uint8_t { Power, Battery, Journey, Charging, AdapterStatus };
DisplayPage activePage = DisplayPage::Power;
bool chargingHome = false;
uint32_t lastPageSelectionMs = 0;
uint32_t pairingResetNoticeUntilMs = 0;
bool pairingConfirmationPending = false;
uint32_t pairingConfirmationUntilMs = 0;
MotGestureEvent pairingConfirmationDirection = MotGestureEvent::None;

void printCentered(const String &text, int16_t y, uint8_t size, uint16_t color) {
  graphics->setTextSize(size);
  graphics->setTextColor(color, kBackground);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  graphics->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  graphics->setCursor((kScreenSize - static_cast<int>(width)) / 2, y);
  graphics->print(text);
}

uint16_t stateColor(FlowState state) {
  switch (state) {
    case FlowState::Consumption:
      return kConsumption;
    case FlowState::Regeneration:
      return kRegen;
    case FlowState::Charging:
      return kCharging;
    case FlowState::Ready:
    default:
      return kMuted;
  }
}

const char *stateLabel(FlowState state) {
  switch (state) {
    case FlowState::Consumption:
      return "VERBRAUCH";
    case FlowState::Regeneration:
      return "REKUPERATION";
    case FlowState::Charging:
      return "LADEN";
    case FlowState::Ready:
    default:
      return "BEREIT";
  }
}

void drawRing(float magnitudeKw, uint16_t color) {
  constexpr int kSegments = 45;
  constexpr float kStartDegrees = 135.0F;
  constexpr float kSweepDegrees = 270.0F;
  constexpr float kMaximumKw = 25.0F;
  const int activeSegments = static_cast<int>(roundf(
      constrain(magnitudeKw, 0.0F, kMaximumKw) / kMaximumKw * kSegments));

  for (int index = 0; index < kSegments; ++index) {
    const float degrees = kStartDegrees +
        (kSweepDegrees * static_cast<float>(index) / static_cast<float>(kSegments - 1));
    const float radians = degrees * PI / 180.0F;
    const int16_t xOuter = kCenter + static_cast<int16_t>(cosf(radians) * 113.0F);
    const int16_t yOuter = kCenter + static_cast<int16_t>(sinf(radians) * 113.0F);
    const int16_t xInner = kCenter + static_cast<int16_t>(cosf(radians) * 103.0F);
    const int16_t yInner = kCenter + static_cast<int16_t>(sinf(radians) * 103.0F);
    graphics->drawLine(xInner, yInner, xOuter, yOuter,
                       index < activeSegments ? color : kTrack);
    graphics->drawLine(xInner + 1, yInner, xOuter + 1, yOuter,
                       index < activeSegments ? color : kTrack);
  }
}

void drawIndicators(const MotDisplayBle::DrivingStatusV1 &driving) {
  if (!(driving.flags & MotDisplayBle::kIndicatorStateValid)) return;
  if (driving.flags & MotDisplayBle::kLeftIndicatorActive) {
    graphics->fillTriangle(19, 108, 37, 96, 37, 120, kConsumption);
    graphics->fillRect(36, 103, 15, 11, kConsumption);
  }
  if (driving.flags & MotDisplayBle::kRightIndicatorActive) {
    graphics->fillTriangle(221, 108, 203, 96, 203, 120, kConsumption);
    graphics->fillRect(189, 103, 15, 11, kConsumption);
  }
}

void render(const MotDisplaySample &sample) {
  MotDisplayBle::DrivingStatusV1 driving{};
  uint32_t drivingAgeMs = 0;
  const bool drivingFresh = motBleClientLatestDriving(driving, drivingAgeMs);
  const bool speedValid = drivingFresh &&
      (driving.flags & MotDisplayBle::kDrivingSpeedValid);
  const uint16_t accent = sample.powerValid ? stateColor(sample.state) : kMuted;
  graphics->fillScreen(kBackground);
  drawRing(fabsf(sample.powerKw), accent);
  if (drivingFresh) drawIndicators(driving);

  printCentered(sample.socValid ? "SOC " + String(sample.socPercent) + "%" : "SOC --", 25, 2,
                kForeground);

  // Keep the BLE contract canonical (energy into the battery is negative), but
  // present charger output as a positive magnitude. Driving regeneration keeps
  // its negative sign so it remains distinct from traction consumption.
  const float displayedPowerKw = sample.state == FlowState::Charging
      ? fabsf(sample.powerKw) : sample.powerKw;
  char power[16];
  if (sample.powerValid) snprintf(power, sizeof(power), "%+.1f", displayedPowerKw);
  else strlcpy(power, "--.-", sizeof(power));
  const String speed = speedValid
      ? String(static_cast<unsigned>(lroundf(driving.speedDeciKmh / 10.0F))) : "--";
  printCentered(speed, 53, 5, speedValid ? kForeground : kMuted);
  printCentered("km/h", 96, 2, speedValid ? kForeground : kMuted);
  printCentered(String(power) + " kW", 128, 2, accent);

  const String range = sample.rangeValid && sample.rangeToTargetKm < UINT16_MAX
      ? String(sample.rangeToTargetKm) + " km bis " + String(sample.targetSocPercent) + "%"
      : "-- km bis --%";
  printCentered(range, 158, 1, kForeground);
  const String footer = sample.powerValid ? stateLabel(sample.state) : motBleClientStatus();
  // Long BLE states such as "SUCHE GEKOPPELTES MOT" exceed the round
  // 240-pixel viewport at size 2. Keep the normal driving labels prominent,
  // but fit diagnostic connection text without clipping the circular edge.
  printCentered(footer, 184, footer.length() > 16 ? 1 : 2, accent);
  canvas->flush();
}

String boundedName(const char *value, const char *fallback) {
  if (!value || !value[0]) return fallback;
  String name(value);
  if (name.length() > 14) name = name.substring(0, 14);
  return name;
}

void renderAdapterStatus() {
  MotDisplayBle::StatusV1 status{};
  uint32_t ageMs = 0;
  const bool valid = motBleClientLatestStatus(status, ageMs);
  graphics->fillScreen(kBackground);
  graphics->drawCircle(kCenter, kCenter, 113, valid ? kCharging : kTrack);
  graphics->drawCircle(kCenter, kCenter, 112, valid ? kCharging : kTrack);
  printCentered("MOT STATUS", 25, 2, kForeground);
  if (!valid) {
    printCentered("WARTE AUF", 91, 2, kMuted);
    printCentered("STATUSDATEN", 116, 2, kMuted);
    const String bleStatus = motBleClientStatus();
    const uint8_t bleStatusSize = bleStatus.length() > 16 ? 1 : 2;
    printCentered(bleStatus, bleStatusSize == 1 ? 180 : 174, bleStatusSize, kMuted);
    printCentered(motGestureAvailable() ? "3s DREHEN = KOPPELN" : "IMU NICHT BEREIT",
                  211, 1, kMuted);
    canvas->flush();
    return;
  }

  const bool wifi1Configured = status.flags & MotDisplayBle::kWifi1Configured;
  const bool wifi2Configured = status.flags & MotDisplayBle::kWifi2Configured;
  const bool wifi1Active = status.flags & MotDisplayBle::kWifi1Active;
  const bool wifi2Active = status.flags & MotDisplayBle::kWifi2Active;
  const bool online = status.flags & MotDisplayBle::kNetworkOnline;
  const bool awsEnabled = status.flags & MotDisplayBle::kAwsEnabled;
  const bool awsConnected = status.flags & MotDisplayBle::kAwsConnected;
  const bool awsRecentlyHealthy = status.flags2 & MotDisplayBle::kAwsRecentlyHealthy;
  const bool awsCredentials = status.flags & MotDisplayBle::kAwsCredentialsReady;
  const bool apActive = status.flags2 & MotDisplayBle::kAccessPointActive;

  printCentered("WIFI 1  " + boundedName(status.wifi1Name, "--"), 55, 2,
                wifi1Active ? kRegen : (wifi1Configured ? kForeground : kMuted));
  printCentered(wifi1Active && online ? "AKTIV  " + String(status.wifiRssi) + " dBm" :
                (wifi1Configured ? "BEREIT, NICHT AKTIV" : "NICHT GESETZT"), 76, 1,
                wifi1Active && online ? kRegen : kMuted);
  printCentered("WIFI 2  " + boundedName(status.wifi2Name, "--"), 100, 2,
                wifi2Active ? kRegen : (wifi2Configured ? kForeground : kMuted));
  printCentered(wifi2Active && online ? "AKTIV  " + String(status.wifiRssi) + " dBm" :
                (wifi2Configured ? "BEREIT, NICHT AKTIV" : "NICHT GESETZT"), 121, 1,
                wifi2Active && online ? kRegen : kMuted);
  const bool ipAvailable = online || apActive;
  printCentered(ipAvailable ? String(apActive && !online ? "AP IP  " : "IP  ") + status.ipAddress :
                "IP  --", 143, 1, ipAvailable ? kForeground : kMuted);
  printCentered(apActive ? "SETUP-AP AKTIV" : "SETUP-AP AUS", 158, 1,
                apActive ? kConsumption : kMuted);
  printCentered("AWS CLOUD", 176, 2, kForeground);
  const char *awsLabel = !awsEnabled ? "AUS" : !online ? "KEIN NETZ" :
      awsConnected ? "VERBUNDEN" : awsRecentlyHealthy ? "ZULETZT OK" :
      awsCredentials ? "GETRENNT" : "NICHT KONFIG.";
  printCentered(awsLabel, 197, 1,
                (awsConnected || awsRecentlyHealthy) ? kRegen :
                (awsEnabled ? kConsumption : kMuted));
  printCentered(motGestureAvailable() ? "3s DREHEN = KOPPELN" : "IMU NICHT BEREIT",
                213, 1, kMuted);
  canvas->flush();
}

bool latestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet) {
  uint32_t ageMs = 0;
  return motBleClientLatestExtended(packet, ageMs);
}

void renderBattery() {
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  const bool valid = latestExtended(packet);
  graphics->fillScreen(kBackground);
  graphics->drawCircle(kCenter, kCenter, 113, valid ? kRegen : kTrack);
  graphics->drawCircle(kCenter, kCenter, 112, valid ? kRegen : kTrack);
  printCentered("TRAKTIONSBATTERIE", 27, 1, kForeground);
  const bool battery = valid && (packet.flags & MotDisplayBle::kBatteryValuesValid);
  const bool soh = valid && (packet.flags & MotDisplayBle::kSohValid);
  const bool delta = valid && (packet.flags & MotDisplayBle::kCellDeltaValid);
  printCentered(soh ? "SOH " + String(packet.sohHundredths / 100.0f, 1) + "%" : "SOH --", 55, 2,
                soh ? kRegen : kMuted);
  printCentered(battery ? "PACK " + String(packet.packVoltageDeciV / 10.0f, 1) + " V" : "PACK -- V",
                91, 2, battery ? kForeground : kMuted);
  printCentered(battery ? "STROM " + String(packet.packCurrentDeciA / 10.0f, 1) + " A" : "STROM -- A",
                124, 2, battery ? kForeground : kMuted);
  printCentered(delta ? "ZELLDIFFERENZ" : "ZELLDIFFERENZ --", 159, 1, delta ? kForeground : kMuted);
  printCentered(delta ? String(packet.cellDeltaMv) + " mV" : "-- mV", 177, 2,
                delta ? kConsumption : kMuted);
  printCentered("DREHEN = SEITE", 211, 1, kMuted);
  canvas->flush();
}

String durationText(uint16_t minutes) {
  if (minutes < 60) return String(minutes) + " min";
  return String(minutes / 60) + "h " + String(minutes % 60) + "min";
}

void renderJourney() {
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  const bool valid = latestExtended(packet);
  const bool journey = valid && (packet.flags & MotDisplayBle::kJourneyValid);
  graphics->fillScreen(kBackground);
  graphics->drawCircle(kCenter, kCenter, 113, journey ? kConsumption : kTrack);
  graphics->drawCircle(kCenter, kCenter, 112, journey ? kConsumption : kTrack);
  printCentered("AKTUELLE FAHRT", 27, 1, kForeground);
  printCentered(journey ? String(packet.journeyDistanceDeciKm / 10.0f, 1) + " km" : "-- km",
                55, 3, journey ? kForeground : kMuted);
  printCentered(journey ? durationText(packet.journeyDurationMinutes) : "-- min", 96, 2,
                journey ? kForeground : kMuted);
  const bool consumption = journey && (packet.flags & MotDisplayBle::kJourneyConsumptionValid);
  printCentered(consumption ? String(packet.journeyConsumptionDeciKwhPer100Km / 10.0f, 1) + " kWh/100" : "-- kWh/100",
                130, 2, consumption ? kConsumption : kMuted);
  const bool peak = journey && (packet.flags & MotDisplayBle::kRegenPeakValid);
  printCentered("REKU MAX", 165, 1, peak ? kRegen : kMuted);
  printCentered(peak ? String(packet.regenPeakDeciKw / 10.0f, 1) + " kW" : "-- kW", 182, 2,
                peak ? kRegen : kMuted);
  printCentered("DREHEN = SEITE", 211, 1, kMuted);
  canvas->flush();
}

void renderCharging(const MotDisplaySample &sample) {
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  const bool valid = latestExtended(packet);
  const bool charging = valid && (packet.flags & MotDisplayBle::kChargeValid);
  graphics->fillScreen(kBackground);
  graphics->drawCircle(kCenter, kCenter, 113, charging ? kCharging : kTrack);
  graphics->drawCircle(kCenter, kCenter, 112, charging ? kCharging : kTrack);
  printCentered("LADEN", 27, 2, kForeground);
  printCentered(charging ? String(packet.chargePowerDeciKw / 10.0f, 1) + " kW" : "-- kW",
                59, 3, charging ? kCharging : kMuted);
  printCentered(sample.socValid ? "SOC " + String(sample.socPercent) + "%" : "SOC --%",
                96, 2, sample.socValid ? kForeground : kMuted);
  const bool target = valid && (packet.flags & MotDisplayBle::kChargeTargetValid);
  printCentered(target ? "ZIEL " + String(packet.chargeTargetSocPercent) + "%" : "ZIEL --%",
                122, 2, target ? kForeground : kMuted);
  const bool remaining = valid && (packet.flags & MotDisplayBle::kChargeRemainingValid);
  printCentered(remaining ? "RESTZEIT " + durationText(packet.chargeRemainingMinutes)
                          : "RESTZEIT --",
                148, 1, remaining ? kForeground : kMuted);
  const bool energy = valid && (packet.flags & MotDisplayBle::kChargeEnergyValid);
  printCentered("GELADEN", 169, 1, energy ? kForeground : kMuted);
  printCentered(energy ? String(packet.chargeEnergyWh / 1000.0f, 2) + " kWh" : "-- kWh",
                186, 2, energy ? kRegen : kMuted);
  printCentered("DREHEN = SEITE", 215, 1, kMuted);
  canvas->flush();
}

DisplayPage homePage() {
  return chargingHome ? DisplayPage::Charging : DisplayPage::Power;
}

DisplayPage nextPage(DisplayPage page) {
  if (chargingHome) {
    switch (page) {
      case DisplayPage::Charging: return DisplayPage::Battery;
      case DisplayPage::Battery: return DisplayPage::Journey;
      case DisplayPage::Journey: return DisplayPage::AdapterStatus;
      case DisplayPage::AdapterStatus: return DisplayPage::Power;
      case DisplayPage::Power: return DisplayPage::Charging;
    }
  } else {
    switch (page) {
      case DisplayPage::Power: return DisplayPage::Journey;
      case DisplayPage::Journey: return DisplayPage::Battery;
      case DisplayPage::Battery: return DisplayPage::Charging;
      case DisplayPage::Charging: return DisplayPage::AdapterStatus;
      case DisplayPage::AdapterStatus: return DisplayPage::Power;
    }
  }
  return homePage();
}

const char *pageName(DisplayPage page) {
  switch (page) {
    case DisplayPage::Power: return "power";
    case DisplayPage::Battery: return "battery";
    case DisplayPage::Journey: return "journey";
    case DisplayPage::Charging: return "charging";
    case DisplayPage::AdapterStatus: return "status";
  }
  return "unknown";
}

void renderPairingResetNotice() {
  graphics->fillScreen(kBackground);
  graphics->drawCircle(kCenter, kCenter, 113, kCharging);
  graphics->drawCircle(kCenter, kCenter, 112, kCharging);
  printCentered("MOT DISPLAY", 47, 2, kForeground);
  printCentered("ZUORDNUNG", 91, 2, kCharging);
  printCentered("GELOESCHT", 116, 2, kCharging);
  printCentered("NEU KOPPELN", 157, 2, kForeground);
  printCentered("SUCHE MOT ...", 188, 1, kMuted);
  canvas->flush();
}

void renderPairingConfirmation() {
  graphics->fillScreen(kBackground);
  graphics->drawCircle(kCenter, kCenter, 113, kConsumption);
  graphics->drawCircle(kCenter, kCenter, 112, kConsumption);
  printCentered("NEU KOPPELN?", 55, 2, kForeground);
  printCentered("ZUR MITTE", 103, 2, kMuted);
  printCentered("3s GEGENRICHTUNG", 137, 2, kConsumption);
  printCentered("ABBRUCH NACH 10s", 183, 1, kMuted);
  canvas->flush();
}

MotDisplaySample liveSample() {
  MotDisplayBle::TelemetryV1 packet{};
  uint32_t receivedAgeMs = 0;
  if (!motBleClientLatest(packet, receivedAgeMs)) {
    return MotDisplaySample{0.0F, 0, UINT16_MAX, 0, FlowState::Ready, false, false, false};
  }

  MotDisplaySample sample{};
  sample.powerKw = packet.powerDeciKw / 10.0F;
  sample.socPercent = packet.socPercent;
  sample.rangeToTargetKm = packet.rangeToTargetKm >= 0
      ? static_cast<uint16_t>(packet.rangeToTargetKm) : UINT16_MAX;
  sample.targetSocPercent = packet.targetSocPercent;
  sample.powerValid = (packet.flags & MotDisplayBle::kDataValid) != 0;
  sample.socValid = (packet.flags & MotDisplayBle::kSocValid) != 0;
  sample.rangeValid = sample.socValid && (packet.flags & MotDisplayBle::kRangeValid) != 0;
  const uint8_t flowFlags = packet.flags & (MotDisplayBle::kCharging |
      MotDisplayBle::kRegenerating | MotDisplayBle::kConsuming);
  if (flowFlags && (flowFlags & (flowFlags - 1U))) sample.powerValid = false;
  if (flowFlags == MotDisplayBle::kCharging) sample.state = FlowState::Charging;
  else if (flowFlags == MotDisplayBle::kRegenerating) sample.state = FlowState::Regeneration;
  else if (flowFlags == MotDisplayBle::kConsuming) sample.state = FlowState::Consumption;
  else sample.state = FlowState::Ready;
  return sample;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.printf("MOT Display C6 BLE %s\n", kFirmwareVersion);
  Serial.printf("Heap before display: %u bytes\n", ESP.getFreeHeap());

  pinMode(kLcdBacklight, OUTPUT);
  digitalWrite(kLcdBacklight, LOW);

  if (!canvas->begin(80000000UL)) {
    Serial.println("ERROR: GC9A01 initialization failed");
    return;
  }

  graphics->fillScreen(kBackground);
  canvas->flush();
  digitalWrite(kLcdBacklight, HIGH);
  Serial.printf("Display ready: %dx%d, free heap: %u bytes\n",
                graphics->width(), graphics->height(), ESP.getFreeHeap());
  motBleClientSetup();
  motGestureSetup();
  Serial.printf("BLE client ready, free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  motBleClientLoop();
  const uint32_t nowMs = millis();
  const MotDisplaySample currentSample = liveSample();
  if (currentSample.powerValid) {
    const bool nowCharging = currentSample.state == FlowState::Charging;
    if (nowCharging != chargingHome) {
      chargingHome = nowCharging;
      activePage = homePage();
      lastPageSelectionMs = nowMs;
    }
  }

  if (pairingConfirmationPending &&
      static_cast<int32_t>(pairingConfirmationUntilMs - nowMs) <= 0) {
    pairingConfirmationPending = false;
    pairingConfirmationDirection = MotGestureEvent::None;
  }

  const bool pairingGestureContext = activePage == DisplayPage::AdapterStatus;
  const MotGestureEvent gesture = motGesturePoll(pairingGestureContext);
  if (pairingGestureContext && !pairingConfirmationPending &&
      gesture != MotGestureEvent::None) {
    pairingConfirmationPending = true;
    pairingConfirmationDirection = gesture;
    pairingConfirmationUntilMs = nowMs + kPairingConfirmationWindowMs;
    lastPageSelectionMs = nowMs;
  } else if (pairingGestureContext && pairingConfirmationPending &&
             gesture != MotGestureEvent::None &&
             gesture != pairingConfirmationDirection) {
    pairingConfirmationPending = false;
    pairingConfirmationDirection = MotGestureEvent::None;
    motBleClientForgetPairing();
    pairingResetNoticeUntilMs = nowMs + kPairingResetNoticeMs;
    lastPageSelectionMs = nowMs;
  } else if (!pairingGestureContext && gesture == MotGestureEvent::TiltRight) {
    activePage = nextPage(activePage);
    lastPageSelectionMs = nowMs;
  } else if (!pairingGestureContext && gesture == MotGestureEvent::TiltLeft) {
    // The order is cyclic; four forward steps are one backward step.
    for (uint8_t step = 0; step < 4; ++step) activePage = nextPage(activePage);
    lastPageSelectionMs = nowMs;
  }

  const uint32_t pageReturnMs = activePage == DisplayPage::AdapterStatus
      ? kStatusPageReturnMs : kPageReturnMs;
  if (activePage != homePage() && nowMs - lastPageSelectionMs >= pageReturnMs) {
    pairingConfirmationPending = false;
    pairingConfirmationDirection = MotGestureEvent::None;
    activePage = homePage();
  }
  static uint32_t lastFrameMs = 0;
  if (nowMs - lastFrameMs < kFrameIntervalMs) {
    delay(5);
    return;
  }
  lastFrameMs = nowMs;
  if (static_cast<int32_t>(pairingResetNoticeUntilMs - nowMs) > 0) {
    renderPairingResetNotice();
  } else if (pairingConfirmationPending) {
    renderPairingConfirmation();
  } else if (activePage == DisplayPage::Power) render(currentSample);
  else if (activePage == DisplayPage::Battery) renderBattery();
  else if (activePage == DisplayPage::Journey) renderJourney();
  else if (activePage == DisplayPage::Charging) renderCharging(currentSample);
  else renderAdapterStatus();

  static uint32_t lastLogMs = 0;
  if (nowMs - lastLogMs >= 5000U) {
    lastLogMs = nowMs;
    MotDisplayBle::TelemetryV1 packet{};
    uint32_t packetAgeMs = 0;
    const bool packetFresh = motBleClientLatest(packet, packetAgeMs);
    MotDisplayBle::StatusV1 status{};
    uint32_t statusAgeMs = 0;
    const bool statusFresh = motBleClientLatestStatus(status, statusAgeMs);
    Serial.printf("prototype alive: uptime=%lus free_heap=%u ble=%s packet=%s seq=%u age_ms=%lu flags=0x%02x status=%s status_seq=%u imu=%s page=%s\n",
                  nowMs / 1000U, ESP.getFreeHeap(), motBleClientStatus(),
                  packetFresh ? "fresh" : "stale",
                  static_cast<unsigned>(packet.sequence),
                  static_cast<unsigned long>(packetAgeMs),
                  static_cast<unsigned>(packet.flags), statusFresh ? "fresh" : "stale",
                  static_cast<unsigned>(status.sequence),
                  motGestureAvailable() ? "ready" : "unavailable",
                  pageName(activePage));
  }
}
