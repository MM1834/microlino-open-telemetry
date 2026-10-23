#include <Arduino.h>
#include <ESP_I2S.h>
#include <lvgl.h>
#include <Preferences.h>
#include <Wire.h>
#include <cmath>

#include "Display_ST77916.h"
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "Touch_CST816.h"
#include "dualeye_fun_assets.h"
#include "mot_ble_client.h"

namespace {

constexpr char kFirmwareVersion[] = "MOT-DISPLAY-1.85-0.4.1-PRE17";
constexpr uint32_t kRefreshMs = 200;
constexpr uint32_t kLongPressPairingMs = 3000;
constexpr uint32_t kSettingsUnlockMs = 1200;
constexpr uint32_t kPageReturnMs = 15000;
constexpr uint32_t kStatusPageReturnMs = 30000;
constexpr uint8_t kSideButtonPin = 0;
constexpr uint32_t kButtonDebounceMs = 35;
constexpr uint32_t kOverspeedDelayMs = 1500;
constexpr uint32_t kWarningBlinkMs = 300;
constexpr uint32_t kBeepRepeatMs = 1200;
constexpr uint32_t kAudioSampleRate = 24000;

constexpr uint32_t kBg = 0x080B0F;
constexpr uint32_t kFg = 0xF2F5F7;
constexpr uint32_t kMuted = 0x70808D;
constexpr uint32_t kTrack = 0x202A31;
constexpr uint32_t kConsumption = 0xFF9D22;
constexpr uint32_t kRegen = 0x20D9B5;
constexpr uint32_t kCharging = 0x2D8CFF;
constexpr uint32_t kWarning = 0xFFD447;
constexpr uint32_t kAlarm = 0xFF4949;

lv_display_t *display = nullptr;
lv_indev_t *touch = nullptr;
lv_obj_t *powerPage = nullptr;
lv_obj_t *statusPage = nullptr;
lv_obj_t *speedControlPage = nullptr;
lv_obj_t *batteryPage = nullptr;
lv_obj_t *journeyPage = nullptr;
lv_obj_t *chargingPage = nullptr;
lv_obj_t *settingsPage = nullptr;
lv_obj_t *motImagePage = nullptr;
lv_obj_t *powerArc = nullptr;
lv_obj_t *socLabel = nullptr;
lv_obj_t *speedLabel = nullptr;
lv_obj_t *powerLabel = nullptr;
lv_obj_t *rangeLabel = nullptr;
lv_obj_t *stateLabel = nullptr;
lv_obj_t *leftLabel = nullptr;
lv_obj_t *rightLabel = nullptr;
lv_obj_t *gearLabel = nullptr;
lv_obj_t *parkingLabel = nullptr;
lv_obj_t *lightLabel = nullptr;
lv_obj_t *modeLabel = nullptr;
lv_obj_t *warningLabel = nullptr;
lv_obj_t *speedWarningRing = nullptr;
lv_obj_t *statusTitle = nullptr;
lv_obj_t *statusBody = nullptr;
lv_obj_t *statusWifi1 = nullptr;
lv_obj_t *statusWifi2 = nullptr;
lv_obj_t *statusNetwork = nullptr;
lv_obj_t *statusAws = nullptr;
lv_obj_t *statusPairingHint = nullptr;
lv_obj_t *settingsTitle = nullptr;
lv_obj_t *languageValue = nullptr;
lv_obj_t *unitsValue = nullptr;
lv_obj_t *sourceValue = nullptr;
lv_obj_t *settingsHint = nullptr;
lv_obj_t *speedControlTitle = nullptr;
lv_obj_t *speedControlLimit = nullptr;
lv_obj_t *speedControlMute = nullptr;
lv_obj_t *speedControlHint = nullptr;
lv_obj_t *batteryTitle = nullptr;
lv_obj_t *batteryBody = nullptr;
lv_obj_t *journeyTitle = nullptr;
lv_obj_t *journeyBody = nullptr;
lv_obj_t *journeyAltitude = nullptr;
lv_obj_t *chargingTitle = nullptr;
lv_obj_t *chargingBody = nullptr;
lv_obj_t *chargingSocArc = nullptr;
lv_obj_t *chargingPowerLabel = nullptr;
lv_obj_t *chargingSocLabel = nullptr;
lv_obj_t *chargingDetailLabel = nullptr;
lv_obj_t *settingsLockLabel = nullptr;
lv_obj_t *vehicleBoardPage = nullptr;
lv_obj_t *vehicleBoardTitle = nullptr;
lv_obj_t *vehicleBoardBody = nullptr;
lv_obj_t *softwareVersionsPage = nullptr;
lv_obj_t *softwareVersionsTitle = nullptr;
lv_obj_t *softwareVersionsBody = nullptr;
lv_obj_t *vehicleBoardChart = nullptr;
lv_chart_series_t *vehicleBoardSeries = nullptr;
enum class Page : uint8_t {
  Power, SpeedControl, Journey, Battery, Charging, VehicleBoard, SoftwareVersions, Status, Settings, MotImage
};
Page currentPage = Page::Power;
uint32_t pairingResetNoticeUntilMs = 0;
bool chargingHome = false;
bool settingsEditMode = false;
uint32_t lastPageSelectionMs = 0;
uint8_t presentationLanguage = MotDisplayBle::kLanguageGerman;
uint8_t presentationUnits = MotDisplayBle::kUnitsMetric;
bool followAdapterConfig = true;
bool presentationConfigDirty = false;
uint16_t appliedAdapterRevision = 0;
bool adapterConfigApplied = false;
uint32_t lastConfigWriteMs = 0;
uint8_t speedLimitValue = 0;
bool speedWarningMuted = false;
uint32_t overspeedStartedMs = 0;
uint32_t lastBlinkMs = 0;
uint32_t lastBeepMs = 0;
bool warningRingVisible = false;
bool audioReady = false;
I2SClass audioI2s;
Preferences uiPreferences;
uint32_t pressedAtMs = 0;
uint16_t pressedX = 0;
uint16_t pressedY = 0;
bool touchWasPressed = false;
bool buttonWasPressed = false;
uint32_t buttonChangedMs = 0;
uint32_t buttonPressedAtMs = 0;
uint32_t lastRefreshMs = 0;

lv_color_t *drawBuffer1 = nullptr;
lv_color_t *drawBuffer2 = nullptr;

constexpr float kKmToMiles = 0.621371192237334F;

void savePresentationPreferences() {
  uiPreferences.putUChar("language", presentationLanguage);
  uiPreferences.putUChar("units", presentationUnits);
  uiPreferences.putBool("followMot", followAdapterConfig);
  uiPreferences.putUChar("speedLimit", speedLimitValue);
  uiPreferences.putBool("speedMute", speedWarningMuted);
}

const char *languageCode() {
  static const char *codes[] = {"DEUTSCH", "ENGLISH", "FRANCAIS", "ITALIANO"};
  return codes[min<uint8_t>(presentationLanguage, 3)];
}

const char *textFor(const char *de, const char *en, const char *fr, const char *it) {
  switch (presentationLanguage) {
    case MotDisplayBle::kLanguageEnglish: return en;
    case MotDisplayBle::kLanguageFrench: return fr;
    case MotDisplayBle::kLanguageItalian: return it;
    default: return de;
  }
}

void loadPage(Page page) {
  if (page != Page::Settings) settingsEditMode = false;
  currentPage = page;
  lastPageSelectionMs = millis();
  lv_screen_load(page == Page::Power ? powerPage :
                 page == Page::SpeedControl ? speedControlPage :
                 page == Page::Journey ? journeyPage :
                 page == Page::Battery ? batteryPage :
                 page == Page::Charging ? chargingPage :
                 page == Page::VehicleBoard ? vehicleBoardPage :
                 page == Page::SoftwareVersions ? softwareVersionsPage :
                 page == Page::Status ? statusPage :
                 page == Page::Settings ? settingsPage : motImagePage);
}

Page homePage() {
  return chargingHome ? Page::Charging : Page::Power;
}

void nextPage() {
  if (chargingHome) {
    loadPage(currentPage == Page::Charging ? Page::Battery :
             currentPage == Page::Battery ? Page::Journey :
             currentPage == Page::Journey ? Page::VehicleBoard :
             currentPage == Page::VehicleBoard ? Page::SoftwareVersions :
             currentPage == Page::SoftwareVersions ? Page::Status :
             currentPage == Page::Status ? Page::Settings :
             currentPage == Page::Settings ? Page::MotImage :
             currentPage == Page::MotImage ? Page::Power :
             currentPage == Page::Power ? Page::SpeedControl : Page::Charging);
    return;
  }
  loadPage(currentPage == Page::Power ? Page::SpeedControl :
           currentPage == Page::SpeedControl ? Page::Journey :
           currentPage == Page::Journey ? Page::Battery :
           currentPage == Page::Battery ? Page::Charging :
           currentPage == Page::Charging ? Page::VehicleBoard :
           currentPage == Page::VehicleBoard ? Page::SoftwareVersions :
           currentPage == Page::SoftwareVersions ? Page::Status :
           currentPage == Page::Status ? Page::Settings :
           currentPage == Page::Settings ? Page::MotImage : Page::Power);
}

bool codecWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(0x18);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool codecRead(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(0x18);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(0x18, 1) != 1) return false;
  value = Wire.read();
  return true;
}

bool initSpeedWarningAudio() {
  // ES8311 sequence for 24 kHz, 16-bit mono and 6.144 MHz MCLK. This mirrors
  // the official Waveshare V2 Arduino example while keeping the amplifier off.
  uint8_t reg = 0;
  if (!codecWrite(0x00, 0x1F)) return false;
  delay(20);
  if (!codecWrite(0x00, 0x00) || !codecWrite(0x00, 0x80) ||
      !codecWrite(0x01, 0x3F) || !codecRead(0x06, reg)) return false;
  if (!codecWrite(0x06, reg & ~0x20U) || !codecRead(0x02, reg)) return false;
  if (!codecWrite(0x02, reg & 0x07U) || !codecWrite(0x03, 0x10) ||
      !codecWrite(0x04, 0x10) || !codecWrite(0x05, 0x00) ||
      !codecRead(0x06, reg) || !codecWrite(0x06, (reg & 0xE0U) | 0x03U) ||
      !codecRead(0x07, reg) || !codecWrite(0x07, reg & 0xC0U) ||
      !codecWrite(0x08, 0xFF) || !codecRead(0x00, reg) ||
      !codecWrite(0x00, reg & 0xBFU) || !codecWrite(0x09, 0x0C) ||
      !codecWrite(0x0A, 0x0C) || !codecWrite(0x0D, 0x01) ||
      !codecWrite(0x0E, 0x02) || !codecWrite(0x12, 0x00) ||
      !codecWrite(0x13, 0x10) || !codecWrite(0x1C, 0x6A) ||
      !codecWrite(0x37, 0x08) || !codecWrite(0x32, 0x73)) return false;

  audioI2s.setPins(48, 38, 47, 39, 2);
  if (!audioI2s.begin(I2S_MODE_STD, kAudioSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) return false;
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  return true;
}

void playWarningBeep() {
  if (!audioReady || speedWarningMuted) return;
  constexpr size_t kSamples = kAudioSampleRate * 70U / 1000U;
  static int16_t samples[kSamples];
  constexpr float kStep = 2.0F * PI * 1100.0F / kAudioSampleRate;
  for (size_t i = 0; i < kSamples; ++i) {
    const float edge = min(1.0F, min(i / 120.0F, (kSamples - 1U - i) / 120.0F));
    samples[i] = static_cast<int16_t>(sinf(kStep * i) * 6500.0F * edge);
  }
  digitalWrite(15, HIGH);
  audioI2s.write(reinterpret_cast<uint8_t *>(samples), sizeof(samples));
  int16_t silence[96]{};
  audioI2s.write(reinterpret_cast<uint8_t *>(silence), sizeof(silence));
  digitalWrite(15, LOW);
}

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                    lv_align_t align, int x, int y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, align, x, y);
  return label;
}

lv_obj_t *makeChoice(lv_obj_t *parent, const char *text, int x, int y) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, 112, 54);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_style_radius(box, 18, 0);
  lv_obj_set_style_bg_color(box, lv_color_hex(kTrack), 0);
  lv_obj_set_style_border_color(box, lv_color_hex(kMuted), 0);
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *label = makeLabel(box, &lv_font_montserrat_20, kFg, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(label, text);
  return label;
}

void flushDisplay(lv_display_t *disp, const lv_area_t *area, uint8_t *pixels) {
  LCD_addWindow(area->x1, area->y1, area->x2, area->y2,
                reinterpret_cast<uint16_t *>(pixels));
  lv_display_flush_ready(disp);
}

uint32_t lvTickMillis() {
  return millis();
}

void handleNavigationAction(uint32_t durationMs) {
  if (currentPage == Page::Status && durationMs >= kLongPressPairingMs) {
    motBleClientForgetPairing();
    pairingResetNoticeUntilMs = millis() + 2500U;
    return;
  }
  if (currentPage == Page::Settings && durationMs >= kSettingsUnlockMs) {
    settingsEditMode = !settingsEditMode;
    lastPageSelectionMs = millis();
    return;
  }
  nextPage();
}

void handleSettingsTouch(uint16_t, uint16_t y) {
  if (y < 125) {
    presentationLanguage = static_cast<uint8_t>((presentationLanguage + 1U) % 4U);
    presentationConfigDirty = followAdapterConfig;
  } else if (y < 205) {
    presentationUnits = presentationUnits == MotDisplayBle::kUnitsMetric
        ? MotDisplayBle::kUnitsUk : MotDisplayBle::kUnitsMetric;
    presentationConfigDirty = followAdapterConfig;
  } else if (y < 280) {
    followAdapterConfig = !followAdapterConfig;
    presentationConfigDirty = followAdapterConfig;
  } else {
    loadPage(Page::Power);
  }
  savePresentationPreferences();
}

void handleSpeedControlTouch(uint16_t x, uint16_t y) {
  if (y >= 80 && y < 145) speedLimitValue = x < 180 ? 30 : 50;
  else if (y >= 145 && y < 210) speedLimitValue = x < 180 ? 60 : 80;
  else if (y >= 210 && y < 280) {
    if (x < 180) speedLimitValue = 0;
    else speedWarningMuted = !speedWarningMuted;
  }
  savePresentationPreferences();
  overspeedStartedMs = 0;
  warningRingVisible = false;
  lv_obj_add_flag(speedWarningRing, LV_OBJ_FLAG_HIDDEN);
  loadPage(Page::Power);
}

void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  Touch_Read_Data();
  const bool pressed = touch_data.points != 0;
  if (pressed) {
    data->point.x = touch_data.x;
    data->point.y = touch_data.y;
    data->state = LV_INDEV_STATE_PRESSED;
    if (!touchWasPressed) {
      pressedAtMs = millis();
      pressedX = touch_data.x;
      pressedY = touch_data.y;
      Serial.printf("Touch down: %u,%u\n", touch_data.x, touch_data.y);
    }
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    if (touchWasPressed) {
      const uint32_t duration = millis() - pressedAtMs;
      Serial.printf("Touch up after %lu ms\n", static_cast<unsigned long>(duration));
      if (currentPage == Page::SpeedControl && duration < kLongPressPairingMs)
        handleSpeedControlTouch(pressedX, pressedY);
      else if (currentPage == Page::Settings && duration >= kSettingsUnlockMs)
        handleNavigationAction(duration);
      else if (currentPage == Page::Settings && settingsEditMode)
        handleSettingsTouch(pressedX, pressedY);
      else
        handleNavigationAction(duration);
    }
  }
  touchWasPressed = pressed;
  touch_data.points = 0;
  touch_data.gesture = NONE;
}

void pollSideButton() {
  const bool pressed = digitalRead(kSideButtonPin) == LOW;
  if (pressed == buttonWasPressed || millis() - buttonChangedMs < kButtonDebounceMs) return;
  buttonChangedMs = millis();
  buttonWasPressed = pressed;
  if (pressed) {
    buttonPressedAtMs = millis();
    Serial.println("KEY1 down");
  } else {
    const uint32_t duration = millis() - buttonPressedAtMs;
    Serial.printf("KEY1 up after %lu ms\n", static_cast<unsigned long>(duration));
    handleNavigationAction(duration);
  }
}

void pageEvent(lv_event_t *event) {
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED) {
    pressedAtMs = millis();
  } else if (code == LV_EVENT_RELEASED) {
    const uint32_t duration = millis() - pressedAtMs;
    if (currentPage == Page::Status && duration >= kLongPressPairingMs) {
      motBleClientForgetPairing();
      pairingResetNoticeUntilMs = millis() + 2500U;
    } else {
      nextPage();
    }
  }
}

void createPowerPage() {
  powerPage = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(powerPage, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(powerPage, 0, 0);
  lv_obj_remove_flag(powerPage, LV_OBJ_FLAG_SCROLLABLE);

  powerArc = lv_arc_create(powerPage);
  lv_obj_set_size(powerArc, 344, 344);
  lv_obj_center(powerArc);
  lv_arc_set_rotation(powerArc, 135);
  lv_arc_set_bg_angles(powerArc, 0, 270);
  lv_arc_set_range(powerArc, 0, 250);
  lv_arc_set_value(powerArc, 0);
  lv_obj_remove_style(powerArc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(powerArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(powerArc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_color(powerArc, lv_color_hex(kTrack), LV_PART_MAIN);
  lv_obj_set_style_arc_width(powerArc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(powerArc, true, LV_PART_INDICATOR);

  socLabel = makeLabel(powerPage, &lv_font_montserrat_24, kFg, LV_ALIGN_TOP_MID, 0, 28);
  leftLabel = makeLabel(powerPage, &lv_font_montserrat_48, kRegen, LV_ALIGN_TOP_LEFT, 35, 94);
  rightLabel = makeLabel(powerPage, &lv_font_montserrat_48, kRegen, LV_ALIGN_TOP_RIGHT, -35, 94);
  gearLabel = makeLabel(powerPage, &lv_font_montserrat_32, kFg, LV_ALIGN_TOP_RIGHT, -76, 57);
  parkingLabel = makeLabel(powerPage, &lv_font_montserrat_24, kAlarm, LV_ALIGN_TOP_LEFT, 74, 60);
  lightLabel = makeLabel(powerPage, &lv_font_montserrat_14, kWarning, LV_ALIGN_LEFT_MID, 34, -18);
  modeLabel = makeLabel(powerPage, &lv_font_montserrat_14, kConsumption, LV_ALIGN_BOTTOM_MID, 0, -31);
  speedLabel = makeLabel(powerPage, &lv_font_montserrat_48, kFg, LV_ALIGN_CENTER, 0, -45);
  speedWarningRing = lv_obj_create(powerPage);
  lv_obj_set_size(speedWarningRing, 166, 140);
  lv_obj_align(speedWarningRing, LV_ALIGN_CENTER, 0, -45);
  lv_obj_set_style_radius(speedWarningRing, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(speedWarningRing, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(speedWarningRing, 7, 0);
  lv_obj_set_style_border_color(speedWarningRing, lv_color_hex(kConsumption), 0);
  lv_obj_clear_flag(speedWarningRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(speedWarningRing, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(speedLabel);
  powerLabel = makeLabel(powerPage, &lv_font_montserrat_28, kMuted, LV_ALIGN_CENTER, 0, 31);
  rangeLabel = makeLabel(powerPage, &lv_font_montserrat_18, kFg, LV_ALIGN_CENTER, 0, 75);
  stateLabel = makeLabel(powerPage, &lv_font_montserrat_20, kMuted, LV_ALIGN_BOTTOM_MID, 0, -56);
  warningLabel = makeLabel(powerPage, &lv_font_montserrat_16, kWarning, LV_ALIGN_BOTTOM_MID, 0, -83);

  lv_label_set_text(socLabel, "SOC --");
  lv_label_set_text(speedLabel, "--\nkm/h");
  lv_label_set_text(powerLabel, "--.- kW");
  lv_label_set_text(rangeLabel, "-- km bis --%");
  lv_label_set_text(stateLabel, "SUCHE MOT");
  lv_label_set_text(leftLabel, "");
  lv_label_set_text(rightLabel, "");
  lv_label_set_text(gearLabel, "");
  lv_label_set_text(parkingLabel, "");
  lv_label_set_text(lightLabel, "");
  lv_label_set_text(modeLabel, "");
  lv_label_set_text(warningLabel, "");
}

void createSpeedControlPage() {
  speedControlPage = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(speedControlPage, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(speedControlPage, 0, 0);
  lv_obj_remove_flag(speedControlPage, LV_OBJ_FLAG_SCROLLABLE);
  speedControlTitle = makeLabel(speedControlPage, &lv_font_montserrat_24, kFg,
                                LV_ALIGN_TOP_MID, 0, 25);
  makeChoice(speedControlPage, "30", 55, 78);
  makeChoice(speedControlPage, "50", 193, 78);
  makeChoice(speedControlPage, "60", 55, 145);
  makeChoice(speedControlPage, "80", 193, 145);
  speedControlLimit = makeChoice(speedControlPage, "AUS", 55, 212);
  speedControlMute = makeChoice(speedControlPage, "MUTE", 193, 212);
  speedControlHint = makeLabel(speedControlPage, &lv_font_montserrat_14, kMuted,
                               LV_ALIGN_BOTTOM_MID, 0, -28);
}

void createDataPage(lv_obj_t *&page, lv_obj_t *&title, lv_obj_t *&body,
                    uint32_t accent) {
  page = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(page, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_color(page, lv_color_hex(accent), 0);
  title = makeLabel(page, &lv_font_montserrat_20, kFg, LV_ALIGN_TOP_MID, 0, 34);
  body = makeLabel(page, &lv_font_montserrat_20, kFg, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_width(body, 304);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
}

void createBatteryPage() {
  createDataPage(batteryPage, batteryTitle, batteryBody, kRegen);
}

void createJourneyPage() {
  createDataPage(journeyPage, journeyTitle, journeyBody, kConsumption);
  lv_obj_align(journeyBody, LV_ALIGN_CENTER, 0, -4);
  journeyAltitude = makeLabel(journeyPage, &lv_font_montserrat_14, kMuted,
                              LV_ALIGN_BOTTOM_MID, 0, -31);
}

void createVehicleBoardPage() {
  createDataPage(vehicleBoardPage, vehicleBoardTitle, vehicleBoardBody, kCharging);
  lv_obj_align(vehicleBoardBody, LV_ALIGN_TOP_MID, 0, 72);
  vehicleBoardChart = lv_chart_create(vehicleBoardPage);
  lv_obj_set_size(vehicleBoardChart, 280, 82);
  lv_obj_align(vehicleBoardChart, LV_ALIGN_BOTTOM_MID, 0, -35);
  lv_chart_set_type(vehicleBoardChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(vehicleBoardChart, MotDisplayBle::kBoardVoltageHistorySamples);
  lv_chart_set_range(vehicleBoardChart, LV_CHART_AXIS_PRIMARY_Y, 1000, 1500);
  lv_obj_set_style_bg_color(vehicleBoardChart, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_color(vehicleBoardChart, lv_color_hex(kTrack), 0);
  lv_obj_set_style_line_color(vehicleBoardChart, lv_color_hex(kTrack), LV_PART_MAIN);
  vehicleBoardSeries = lv_chart_add_series(
      vehicleBoardChart, lv_color_hex(kCharging), LV_CHART_AXIS_PRIMARY_Y);
}

void createSoftwareVersionsPage() {
  createDataPage(softwareVersionsPage, softwareVersionsTitle, softwareVersionsBody, kRegen);
  lv_obj_set_style_text_font(softwareVersionsTitle, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_align(softwareVersionsTitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(softwareVersionsTitle, LV_ALIGN_TOP_MID, 0, 24);
  lv_obj_align(softwareVersionsBody, LV_ALIGN_CENTER, 0, 20);
}

void createChargingPage() {
  chargingPage = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(chargingPage, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(chargingPage, 0, 0);
  lv_obj_remove_flag(chargingPage, LV_OBJ_FLAG_SCROLLABLE);

  chargingSocArc = lv_arc_create(chargingPage);
  lv_obj_set_size(chargingSocArc, 344, 344);
  lv_obj_center(chargingSocArc);
  lv_arc_set_rotation(chargingSocArc, 135);
  lv_arc_set_bg_angles(chargingSocArc, 0, 270);
  lv_arc_set_range(chargingSocArc, 0, 100);
  lv_arc_set_value(chargingSocArc, 0);
  lv_obj_remove_style(chargingSocArc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(chargingSocArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(chargingSocArc, 15, LV_PART_MAIN);
  lv_obj_set_style_arc_color(chargingSocArc, lv_color_hex(kTrack), LV_PART_MAIN);
  lv_obj_set_style_arc_width(chargingSocArc, 15, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(chargingSocArc, true, LV_PART_INDICATOR);

  chargingTitle = makeLabel(chargingPage, &lv_font_montserrat_24, kFg,
                            LV_ALIGN_TOP_MID, 0, 31);
  chargingPowerLabel = makeLabel(chargingPage, &lv_font_montserrat_48, kRegen,
                                 LV_ALIGN_CENTER, 0, -63);
  chargingSocLabel = makeLabel(chargingPage, &lv_font_montserrat_32, kFg,
                               LV_ALIGN_CENTER, 0, 2);
  chargingDetailLabel = makeLabel(chargingPage, &lv_font_montserrat_18, kFg,
                                  LV_ALIGN_CENTER, 0, 76);
  lv_obj_set_width(chargingDetailLabel, 288);
  lv_label_set_long_mode(chargingDetailLabel, LV_LABEL_LONG_WRAP);
}

void createStatusPage() {
  statusPage = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(statusPage, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(statusPage, 0, 0);
  lv_obj_remove_flag(statusPage, LV_OBJ_FLAG_SCROLLABLE);
  statusTitle = makeLabel(statusPage, &lv_font_montserrat_24, kFg, LV_ALIGN_TOP_MID, 0, 49);
  statusBody = makeLabel(statusPage, &lv_font_montserrat_18, kFg, LV_ALIGN_CENTER, 0, 13);
  lv_obj_set_width(statusBody, 300);
  lv_label_set_long_mode(statusBody, LV_LABEL_LONG_WRAP);
  statusWifi1 = makeLabel(statusPage, &lv_font_montserrat_16, kFg,
                          LV_ALIGN_TOP_MID, 0, 82);
  statusWifi2 = makeLabel(statusPage, &lv_font_montserrat_16, kFg,
                          LV_ALIGN_TOP_MID, 0, 132);
  statusNetwork = makeLabel(statusPage, &lv_font_montserrat_16, kFg,
                            LV_ALIGN_TOP_MID, 0, 185);
  statusAws = makeLabel(statusPage, &lv_font_montserrat_18, kFg,
                        LV_ALIGN_TOP_MID, 0, 253);
  statusPairingHint = makeLabel(statusPage, &lv_font_montserrat_14, kMuted,
                                LV_ALIGN_BOTTOM_MID, 0, -62);
  lv_label_set_text(statusTitle, "MOT STATUS");
  lv_label_set_text(statusBody, "SUCHE MOT\n\nTippen: Fahranzeige\n3 s halten: neu koppeln");
  makeLabel(statusPage, &lv_font_montserrat_14, kMuted, LV_ALIGN_BOTTOM_MID, 0, -42);
  lv_obj_t *version = lv_obj_get_child(statusPage, -1);
  lv_label_set_text(version, kFirmwareVersion);
}

void createSettingsPage() {
  settingsPage = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(settingsPage, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(settingsPage, 0, 0);
  lv_obj_remove_flag(settingsPage, LV_OBJ_FLAG_SCROLLABLE);
  settingsTitle = makeLabel(settingsPage, &lv_font_montserrat_24, kFg,
                            LV_ALIGN_TOP_MID, 0, 25);
  settingsLockLabel = makeLabel(settingsPage, &lv_font_montserrat_14, kAlarm,
                                LV_ALIGN_TOP_MID, 0, 59);
  makeLabel(settingsPage, &lv_font_montserrat_14, kMuted, LV_ALIGN_TOP_MID, 0, 87);
  lv_obj_t *languageCaption = lv_obj_get_child(settingsPage, -1);
  languageValue = makeLabel(settingsPage, &lv_font_montserrat_20, kFg,
                            LV_ALIGN_TOP_MID, 0, 106);
  makeLabel(settingsPage, &lv_font_montserrat_14, kMuted, LV_ALIGN_TOP_MID, 0, 158);
  lv_obj_t *unitsCaption = lv_obj_get_child(settingsPage, -1);
  unitsValue = makeLabel(settingsPage, &lv_font_montserrat_20, kFg,
                         LV_ALIGN_TOP_MID, 0, 177);
  makeLabel(settingsPage, &lv_font_montserrat_14, kMuted, LV_ALIGN_TOP_MID, 0, 229);
  lv_obj_t *sourceCaption = lv_obj_get_child(settingsPage, -1);
  sourceValue = makeLabel(settingsPage, &lv_font_montserrat_20, kFg,
                          LV_ALIGN_TOP_MID, 0, 248);
  settingsHint = makeLabel(settingsPage, &lv_font_montserrat_14, kMuted,
                           LV_ALIGN_BOTTOM_MID, 0, -32);
  lv_label_set_text(languageCaption, "SPRACHE");
  lv_label_set_text(unitsCaption, "EINHEITEN");
  lv_label_set_text(sourceCaption, "QUELLE");
}

void createMotImagePage() {
  motImagePage = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(motImagePage, lv_color_hex(kBg), 0);
  lv_obj_set_style_border_width(motImagePage, 0, 0);
  lv_obj_remove_flag(motImagePage, LV_OBJ_FLAG_SCROLLABLE);

  static lv_image_dsc_t imageDescriptor{};
  imageDescriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  imageDescriptor.header.cf = LV_COLOR_FORMAT_RGB565;
  imageDescriptor.header.flags = 0;
  imageDescriptor.header.w = 240;
  imageDescriptor.header.h = 240;
  imageDescriptor.header.stride = 240 * sizeof(uint16_t);
  imageDescriptor.data_size = sizeof(kDualEyeMotVector);
  imageDescriptor.data = reinterpret_cast<const uint8_t *>(kDualEyeMotVector);

  lv_obj_t *image = lv_image_create(motImagePage);
  lv_image_set_src(image, &imageDescriptor);
  lv_image_set_scale(image, 384);  // 240 px -> 360 px
  lv_obj_center(image);
}

void updateSpeedControlPage() {
  lv_label_set_text(speedControlTitle,
      textFor("TEMPOLIMIT", "SPEED LIMIT", "LIMITE VITESSE", "LIMITE VELOCITA"));
  lv_label_set_text(speedControlLimit,
      textFor("AUS", "OFF", "ARRET", "SPENTO"));
  lv_label_set_text(speedControlMute, speedWarningMuted ?
      textFor("TON AN", "SOUND ON", "SON ACTIF", "SUONO ON") : "MUTE");
  lv_label_set_text_fmt(speedControlHint, "%s | %s",
      presentationUnits == MotDisplayBle::kUnitsUk ? "mph" : "km/h",
      textFor("Auswahl = zurueck", "Select = back", "Choix = retour", "Scelta = indietro"));

  const uint8_t values[] = {30, 50, 60, 80};
  for (uint32_t index = 0; index < 4; ++index) {
    lv_obj_t *box = lv_obj_get_child(speedControlPage, index + 1);
    lv_obj_set_style_border_color(box,
        lv_color_hex(speedLimitValue == values[index] ? kConsumption : kMuted), 0);
  }
  lv_obj_set_style_border_color(lv_obj_get_parent(speedControlLimit),
      lv_color_hex(speedLimitValue == 0 ? kConsumption : kMuted), 0);
  lv_obj_set_style_border_color(lv_obj_get_parent(speedControlMute),
      lv_color_hex(speedWarningMuted ? kAlarm : kMuted), 0);
}

String fixedTenths(int32_t value, bool forcePlus = false) {
  const bool negative = value < 0;
  const uint32_t magnitude = static_cast<uint32_t>(negative ? -value : value);
  String result = negative ? "-" : (forcePlus ? "+" : "");
  result += String(magnitude / 10U);
  result += ".";
  result += String(magnitude % 10U);
  return result;
}

String durationText(uint16_t minutes) {
  if (minutes < 60) return String(minutes) + " min";
  return String(minutes / 60) + "h " + String(minutes % 60) + "min";
}

bool latestExtended(MotDisplayBle::ExtendedTelemetryV1 &packet) {
  uint32_t ageMs = 0;
  return motBleClientLatestExtended(packet, ageMs);
}

void updateBatteryPage() {
  lv_label_set_text(batteryTitle,
      textFor("TRAKTIONSBATTERIE", "TRACTION BATTERY", "BATTERIE TRACTION", "BATTERIA TRAZIONE"));
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  const bool valid = latestExtended(packet);
  const bool battery = valid && (packet.flags & MotDisplayBle::kBatteryValuesValid);
  const bool soh = valid && (packet.flags & MotDisplayBle::kSohValid);
  const bool delta = valid && (packet.flags & MotDisplayBle::kCellDeltaValid);
  const String sohText = soh ? fixedTenths((packet.sohHundredths + 5U) / 10U) + "%" : "--";
  const String voltage = battery ? fixedTenths(packet.packVoltageDeciV) + " V" : "-- V";
  const String current = battery ? fixedTenths(packet.packCurrentDeciA, true) + " A" : "-- A";
  const String cellDelta = delta ? String(packet.cellDeltaMv) + " mV" : "-- mV";
  const String body = "SOH  " + sohText + "\n\nPACK  " + voltage +
      "\n\n" + textFor("STROM", "CURRENT", "COURANT", "CORRENTE") + "  " + current +
      "\n\n" + textFor("ZELLDIFFERENZ", "CELL DELTA", "ECART CELLULE", "DELTA CELLE") +
      "  " + cellDelta;
  lv_label_set_text(batteryBody, body.c_str());
}

void updateVehicleBoardPage() {
  lv_label_set_text(vehicleBoardTitle,
      textFor("FAHRZEUG & 12 V", "VEHICLE & 12 V", "VEHICULE & 12 V", "VEICOLO & 12 V"));
  MotDisplayBle::VehicleBoardNetworkV1 packet{};
  uint32_t ageMs = 0;
  const bool fresh = motBleClientLatestVehicleBoardNetwork(packet, ageMs);
  auto pressure = [&](uint16_t flag, uint8_t index) -> String {
    return fresh && (packet.flags & flag)
        ? String(packet.tyrePressureKpa[index] / 100.0f, 2) : String("--");
  };
  auto pressureState = [&](uint16_t flag, uint8_t index) -> uint8_t {
    if (!fresh || !(packet.flags & flag)) return 0;
    const uint16_t value = packet.tyrePressureKpa[index];
    if (value < MotDisplayBle::kTyreLowAlarmKpa || value > MotDisplayBle::kTyreHighAlarmKpa)
      return 2;
    if (value < MotDisplayBle::kTyreLowWarningKpa || value >= MotDisplayBle::kTyreHighWarningKpa)
      return 1;
    return 0;
  };
  const bool voltage = fresh && (packet.flags & MotDisplayBle::kBoardVoltageValid);
  const bool bounds = fresh && (packet.flags & MotDisplayBle::kBoardVoltageBoundsValid);
  const bool alarm = voltage && (packet.flags &
      (MotDisplayBle::kBoardVoltageLowAlarm | MotDisplayBle::kBoardVoltageHighAlarm));
  const bool warning = voltage && (packet.flags &
      (MotDisplayBle::kBoardVoltageLowWarning | MotDisplayBle::kBoardVoltageHighWarning));
  uint8_t tyreState = 0;
  tyreState = max(tyreState, pressureState(MotDisplayBle::kTyreFrontLeftValid, 0));
  tyreState = max(tyreState, pressureState(MotDisplayBle::kTyreFrontRightValid, 1));
  tyreState = max(tyreState, pressureState(MotDisplayBle::kTyreRearLeftValid, 2));
  tyreState = max(tyreState, pressureState(MotDisplayBle::kTyreRearRightValid, 3));
  const uint32_t voltageColor = (alarm || tyreState == 2) ? kAlarm
      : (warning || tyreState == 1) ? kWarning : kRegen;
  const String state = alarm
      ? textFor("ALARM", "ALARM", "ALARME", "ALLARME")
      : warning ? textFor("GEL PRUEFEN", "CHECK GEL", "VERIFIER GEL", "CONTROLLA GEL")
      : voltage ? "GEL OK" : "--";
  const String body = "VL " + pressure(MotDisplayBle::kTyreFrontLeftValid, 0) +
      "   VR " + pressure(MotDisplayBle::kTyreFrontRightValid, 1) + " bar\n" +
      "HL " + pressure(MotDisplayBle::kTyreRearLeftValid, 2) +
      "   HR " + pressure(MotDisplayBle::kTyreRearRightValid, 3) + " bar\n\n" +
      "12 V  " + (voltage ? String(packet.voltageCentiV / 100.0f, 2) : "--") + " V    " +
      "MIN " + (bounds ? String(packet.minimumCentiV / 100.0f, 2) : "--") + "  " +
      "MAX " + (bounds ? String(packet.maximumCentiV / 100.0f, 2) : "--") +
      "\n" + state;
  lv_label_set_text(vehicleBoardBody, body.c_str());
  lv_obj_set_style_text_color(vehicleBoardBody, lv_color_hex(voltageColor), 0);

  const bool history = fresh && (packet.flags & MotDisplayBle::kBoardVoltageHistoryValid) &&
      packet.historyCount > 1;
  for (uint8_t index = 0; index < MotDisplayBle::kBoardVoltageHistorySamples; ++index) {
    const bool present = history && index >= MotDisplayBle::kBoardVoltageHistorySamples -
        packet.historyCount;
    const uint8_t source = present
        ? index - (MotDisplayBle::kBoardVoltageHistorySamples - packet.historyCount) : 0;
    lv_chart_set_value_by_id(vehicleBoardChart, vehicleBoardSeries, index,
        present ? packet.historyCentiV[source] : LV_CHART_POINT_NONE);
  }
  lv_chart_refresh(vehicleBoardChart);
}

void updateSoftwareVersionsPage() {
  lv_label_set_text(softwareVersionsTitle,
      textFor("SOFTWARE-\nVERSIONEN", "SOFTWARE\nVERSIONS",
              "VERSIONS\nLOGICIELLES", "VERSIONI\nSOFTWARE"));
  MotDisplayBle::SoftwareVersionsV1 packet{};
  uint32_t ageMs = 0;
  const bool fresh = motBleClientLatestSoftwareVersions(packet, ageMs);
  const auto version = [](uint8_t major, uint8_t minor, uint16_t patch) {
    return String(major) + "." + String(minor) + "." + String(patch);
  };
  const String vcu = fresh && (packet.flags & MotDisplayBle::kVcuSoftwareVersionValid)
      ? version(packet.vcuMajor, packet.vcuMinor, packet.vcuBuild) : "--";
  const String ipc = fresh && (packet.flags & MotDisplayBle::kIpcSoftwareVersionValid)
      ? version(packet.ipcMajor, packet.ipcMinor, packet.ipcPatch) : "--";
  const String bdu = fresh && (packet.flags & MotDisplayBle::kBduSoftwareVersionValid)
      ? version(packet.bduMajor, packet.bduMinor, packet.bduPatch) : "--";
  const String body = "VCU   " + vcu + "\n\nIPC   " + ipc + "\n\nBDU   " + bdu +
      "\n\n" + textFor("Passiv empfangen", "Received passively",
                        "Recu passivement", "Ricevuto passivamente");
  lv_label_set_text(softwareVersionsBody, body.c_str());
}

void updateJourneyPage() {
  lv_label_set_text(journeyTitle,
      textFor("AKTUELLE FAHRT", "CURRENT JOURNEY", "TRAJET ACTUEL", "VIAGGIO ATTUALE"));
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  const bool valid = latestExtended(packet);
  const bool journey = valid && (packet.flags & MotDisplayBle::kJourneyValid);
  const bool consumption = journey && (packet.flags & MotDisplayBle::kJourneyConsumptionValid);
  const bool peak = journey && (packet.flags & MotDisplayBle::kRegenPeakValid);
  int32_t distanceDeci = packet.journeyDistanceDeciKm;
  if (presentationUnits == MotDisplayBle::kUnitsUk)
    distanceDeci = lroundf(distanceDeci * kKmToMiles);
  const String body = (journey ? fixedTenths(distanceDeci) : "--") + " " +
      (presentationUnits == MotDisplayBle::kUnitsUk ? "mi" : "km") + "\n\n" +
      (journey ? durationText(packet.journeyDurationMinutes) : "-- min") + "\n\n" +
      textFor("VERBRAUCH", "CONSUMPTION", "CONSOMMATION", "CONSUMO") + "  " +
      (consumption ? fixedTenths(packet.journeyConsumptionDeciKwhPer100Km) : "--") +
      " kWh/100\n\n" + textFor("REKU MAX", "REGEN MAX", "REGEN MAX", "RECUPERO MAX") +
      "  " + (peak ? fixedTenths(packet.regenPeakDeciKw) : "--") + " kW";
  lv_label_set_text(journeyBody, body.c_str());

  MotDisplayBle::JourneyAltitudeV1 altitude{};
  uint32_t altitudeAgeMs = 0;
  const bool altitudePacket = motBleClientLatestJourneyAltitude(altitude, altitudeAgeMs);
  const bool altitudeValid = altitudePacket &&
      (altitude.flags & MotDisplayBle::kJourneyAltitudeValid) &&
      altitude.sourceAgeMs <= 5000U;
  if (!altitudeValid) {
    lv_label_set_text(journeyAltitude, "");
    return;
  }
  const bool uk = presentationUnits == MotDisplayBle::kUnitsUk;
  const float altitudeScale = uk ? 3.28084f : 1.0f;
  const long shownAltitude = lroundf(altitude.altitudeMeters * altitudeScale);
  String altitudeText = textFor("HOEHE", "ALTITUDE", "ALTITUDE", "ALTITUDINE") +
      String("  ") + String(shownAltitude) + (uk ? " ft" : " m");
  if (altitude.flags & MotDisplayBle::kJourneyElevationGainValid) {
    const unsigned long ascent = static_cast<unsigned long>(
        lroundf(altitude.ascentMeters * altitudeScale));
    const unsigned long descent = static_cast<unsigned long>(
        lroundf(altitude.descentMeters * altitudeScale));
    altitudeText += "   +" + String(ascent) + " / -" + String(descent) +
        (uk ? " ft" : " m");
  }
  lv_label_set_text(journeyAltitude, altitudeText.c_str());
}

void updateChargingPage() {
  MotDisplayBle::TelemetryV1 telemetry{};
  MotDisplayBle::ExtendedTelemetryV1 packet{};
  MotDisplayBle::ChargeSummaryV1 summary{};
  uint32_t ageMs = 0;
  uint32_t summaryAgeMs = 0;
  const bool telemetryFresh = motBleClientLatest(telemetry, ageMs);
  const bool valid = latestExtended(packet);
  const bool charging = valid && (packet.flags & MotDisplayBle::kChargeValid);
  const bool target = valid && (packet.flags & MotDisplayBle::kChargeTargetValid);
  const bool remaining = valid && (packet.flags & MotDisplayBle::kChargeRemainingValid);
  const bool energy = valid && (packet.flags & MotDisplayBle::kChargeEnergyValid);
  const bool socValid = telemetryFresh && (telemetry.flags & MotDisplayBle::kSocValid);
  const uint8_t soc = socValid ? min<uint8_t>(telemetry.socPercent, 100) : 0;
  lv_arc_set_value(chargingSocArc, soc);
  const uint32_t socColor = !socValid ? kMuted : soc <= 10 ? kAlarm :
      soc < 35 ? kConsumption : soc < 80 ? kWarning : kRegen;
  lv_obj_set_style_arc_color(chargingSocArc, lv_color_hex(socColor), LV_PART_INDICATOR);

  const bool telemetryChargingPower = telemetryFresh &&
      (telemetry.flags & MotDisplayBle::kDataValid) &&
      (telemetry.flags & MotDisplayBle::kCharging);
  const bool activeCharging = charging || telemetryChargingPower;
  const bool summaryAvailable = motBleClientLatestChargeSummary(summary, summaryAgeMs) &&
      (summary.flags & MotDisplayBle::kChargeSummaryValid);

  if (!activeCharging && summaryAvailable) {
    lv_label_set_text(chargingTitle,
        textFor("LETZTE LADUNG", "LAST CHARGE", "DERNIERE CHARGE", "ULTIMA RICARICA"));
    const bool summarySocValid = summary.flags & MotDisplayBle::kChargeSummarySocValid;
    const bool summaryEnergyValid = summary.flags & MotDisplayBle::kChargeSummaryEnergyValid;
    const bool summaryDurationValid = summary.flags & MotDisplayBle::kChargeSummaryDurationValid;
    const uint8_t endSoc = summarySocValid ? min<uint8_t>(summary.endSocPercent, 100) : 0;
    lv_arc_set_value(chargingSocArc, endSoc);
    const uint32_t summarySocColor = !summarySocValid ? kMuted : endSoc <= 10 ? kAlarm :
        endSoc < 35 ? kConsumption : endSoc < 80 ? kWarning : kRegen;
    lv_obj_set_style_arc_color(chargingSocArc, lv_color_hex(summarySocColor), LV_PART_INDICATOR);

    String energyText = "-- kWh";
    if (summaryEnergyValid) {
      const uint32_t hundredthsKwh = summary.energyChargedWh / 10U;
      energyText = String(hundredthsKwh / 100U) + "." +
          (hundredthsKwh % 100U < 10U ? "0" : "") + String(hundredthsKwh % 100U) + " kWh";
    }
    lv_label_set_text(chargingPowerLabel, energyText.c_str());
    lv_obj_set_style_text_color(chargingPowerLabel, lv_color_hex(kRegen), 0);

    String socText = "SOC --%";
    if (summarySocValid) {
      socText = "SOC " + String(summary.startSocPercent) + " -> " +
          String(summary.endSocPercent) + "%";
    }
    lv_label_set_text(chargingSocLabel, socText.c_str());
    lv_obj_set_style_text_color(chargingSocLabel, lv_color_hex(summarySocColor), 0);

    const int16_t socGain = summarySocValid
        ? static_cast<int16_t>(summary.endSocPercent) - summary.startSocPercent : 0;
    const String body = textFor("SOC GEWINN", "SOC GAIN", "GAIN SOC", "AUMENTO SOC") +
        String("  ") + (summarySocValid ? String(socGain >= 0 ? "+" : "") +
        String(socGain) + "%" : "--%") + "\n\n" +
        textFor("DAUER", "DURATION", "DUREE", "DURATA") + "  " +
        (summaryDurationValid ? durationText(summary.durationMinutes) : "--") + "\n\n" +
        textFor("ABGESCHLOSSEN", "COMPLETED", "TERMINEE", "COMPLETATA");
    lv_label_set_text(chargingDetailLabel, body.c_str());
    return;
  }

  lv_label_set_text(chargingTitle,
      textFor("LADEN", "CHARGING", "CHARGE", "RICARICA"));
  const int32_t chargePowerDeciKw = charging ? packet.chargePowerDeciKw :
      (telemetryChargingPower ? abs(telemetry.powerDeciKw) : 0);
  const String powerText = activeCharging
      ? fixedTenths(chargePowerDeciKw) + " kW" : "-- kW";
  lv_label_set_text(chargingPowerLabel, powerText.c_str());
  lv_obj_set_style_text_color(chargingPowerLabel, lv_color_hex(kRegen), 0);
  const String socText = socValid ? "SOC " + String(soc) + "%" : "SOC --%";
  lv_label_set_text(chargingSocLabel, socText.c_str());
  lv_obj_set_style_text_color(chargingSocLabel, lv_color_hex(socColor), 0);

  String energyText = "-- kWh";
  if (energy) {
    const uint32_t hundredthsKwh = packet.chargeEnergyWh / 10U;
    energyText = String(hundredthsKwh / 100U) + "." +
        (hundredthsKwh % 100U < 10U ? "0" : "") + String(hundredthsKwh % 100U) + " kWh";
  }
  const String body = textFor("ZIEL", "TARGET", "CIBLE", "OBIETTIVO") + String("  ") +
      (target ? String(packet.chargeTargetSocPercent) + "%" : "--%") + "\n\n" +
      textFor("RESTZEIT", "REMAINING", "RESTANT", "RIMANENTE") + "  " +
      (remaining ? durationText(packet.chargeRemainingMinutes) : "--") + "\n\n" +
      textFor("GELADEN", "CHARGED", "CHARGE", "CARICATO") + "  " + energyText;
  lv_label_set_text(chargingDetailLabel, body.c_str());
}

void updateSettingsPage() {
  lv_label_set_text(settingsTitle,
      textFor("EINSTELLUNGEN", "SETTINGS", "REGLAGES", "IMPOSTAZIONI"));
  lv_obj_t *languageCaption = lv_obj_get_child(settingsPage, 2);
  lv_obj_t *unitsCaption = lv_obj_get_child(settingsPage, 4);
  lv_obj_t *sourceCaption = lv_obj_get_child(settingsPage, 6);
  lv_label_set_text(languageCaption,
      textFor("SPRACHE", "LANGUAGE", "LANGUE", "LINGUA"));
  lv_label_set_text(unitsCaption,
      textFor("EINHEITEN", "UNITS", "UNITES", "UNITA"));
  lv_label_set_text(sourceCaption,
      textFor("QUELLE", "SOURCE", "SOURCE", "FONTE"));
  lv_label_set_text(languageValue, languageCode());
  lv_label_set_text(unitsValue, presentationUnits == MotDisplayBle::kUnitsUk
      ? "UK / mi" : "METRISCH / km");
  lv_label_set_text(sourceValue, followAdapterConfig ? "MOT SYNC" :
      textFor("LOKAL", "LOCAL", "LOCAL", "LOCALE"));
  lv_label_set_text(settingsLockLabel, settingsEditMode ?
      textFor("SCHLUESSEL GRUEN - FREI", "GREEN KEY - UNLOCKED",
              "CLE VERTE - OUVERT", "CHIAVE VERDE - APERTA") :
      textFor("SCHLUESSEL ROT - GESPERRT", "RED KEY - LOCKED",
              "CLE ROUGE - BLOQUE", "CHIAVE ROSSA - BLOCCATA"));
  lv_obj_set_style_text_color(settingsLockLabel,
      lv_color_hex(settingsEditMode ? kRegen : kAlarm), 0);
  const uint32_t valueColor = settingsEditMode ? kFg : kMuted;
  lv_obj_set_style_text_color(languageValue, lv_color_hex(valueColor), 0);
  lv_obj_set_style_text_color(unitsValue, lv_color_hex(valueColor), 0);
  lv_obj_set_style_text_color(sourceValue, lv_color_hex(valueColor), 0);
  lv_label_set_text(settingsHint, settingsEditMode ?
      textFor("Zeile tippen | lang: sperren", "Tap row | hold: lock",
              "Toucher | maintenir: bloquer", "Tocca | tieni: blocca") :
      textFor("Lang druecken: entsperren", "Hold to unlock",
              "Maintenir pour ouvrir", "Tieni per sbloccare"));
}

void syncPresentationConfig() {
  MotDisplayBle::PresentationConfigV1 config{};
  uint32_t age = 0;
  const bool available = motBleClientLatestPresentationConfig(config, age);
  if (followAdapterConfig && available && !presentationConfigDirty &&
      (!adapterConfigApplied || config.revision != appliedAdapterRevision)) {
    presentationLanguage = config.language;
    presentationUnits = config.unitSystem;
    appliedAdapterRevision = config.revision;
    adapterConfigApplied = true;
    savePresentationPreferences();
    Serial.printf("Presentation config received: language=%u units=%u revision=%u\n",
                  presentationLanguage, presentationUnits, appliedAdapterRevision);
  }
  if (followAdapterConfig && presentationConfigDirty &&
      millis() - lastConfigWriteMs >= 1000U) {
    lastConfigWriteMs = millis();
    if (motBleClientWritePresentationConfig(presentationLanguage, presentationUnits))
      Serial.println("Presentation config sent to MOT");
  }
  if (presentationConfigDirty && available &&
      config.language == presentationLanguage && config.unitSystem == presentationUnits) {
    presentationConfigDirty = false;
    appliedAdapterRevision = config.revision;
    adapterConfigApplied = true;
    savePresentationPreferences();
  }
}

const char *flowLabel(const MotDisplayBle::TelemetryV1 &telemetry) {
  if (telemetry.flags & MotDisplayBle::kCharging)
    return textFor("LADEN", "CHARGING", "CHARGE", "RICARICA");
  if (telemetry.flags & MotDisplayBle::kRegenerating)
    return textFor("REKUPERATION", "REGENERATION", "RECUPERATION", "RECUPERO");
  if (telemetry.flags & MotDisplayBle::kConsuming)
    return textFor("VERBRAUCH", "CONSUMPTION", "CONSOMMATION", "CONSUMO");
  return textFor("BEREIT", "READY", "PRET", "PRONTO");
}

uint32_t flowColor(const MotDisplayBle::TelemetryV1 &telemetry) {
  if (telemetry.flags & MotDisplayBle::kCharging) return kCharging;
  if (telemetry.flags & MotDisplayBle::kRegenerating) return kRegen;
  if (telemetry.flags & MotDisplayBle::kConsuming) return kConsumption;
  return kMuted;
}

void updateSpeedWarning(bool speedValid, const MotDisplayBle::DrivingStatusV1 &driving) {
  if (!speedValid || speedLimitValue == 0) {
    overspeedStartedMs = 0;
    warningRingVisible = false;
    lv_obj_add_flag(speedWarningRing, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const float speed = driving.speedDeciKmh / 10.0F *
      (presentationUnits == MotDisplayBle::kUnitsUk ? kKmToMiles : 1.0F);
  const float toleranceKmh = speedLimitValue == 30 ? 2.0F : 3.0F;
  const float tolerance = presentationUnits == MotDisplayBle::kUnitsUk
                              ? toleranceKmh * kKmToMiles
                              : toleranceKmh;
  if (speed <= speedLimitValue + tolerance) {
    overspeedStartedMs = 0;
    warningRingVisible = false;
    lv_obj_add_flag(speedWarningRing, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const uint32_t now = millis();
  if (!overspeedStartedMs) overspeedStartedMs = now;
  if (now - overspeedStartedMs < kOverspeedDelayMs) return;
  if (now - lastBlinkMs >= kWarningBlinkMs) {
    lastBlinkMs = now;
    warningRingVisible = !warningRingVisible;
    if (warningRingVisible) lv_obj_remove_flag(speedWarningRing, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(speedWarningRing, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_border_color(speedWarningRing,
      lv_color_hex(speed >= speedLimitValue + 10.0F ? kAlarm : kConsumption), 0);
  if (now - lastBeepMs >= kBeepRepeatMs) {
    lastBeepMs = now;
    playWarningBeep();
  }
}

void updatePowerPage() {
  MotDisplayBle::TelemetryV1 telemetry{};
  MotDisplayBle::DrivingStatusV1 driving{};
  uint32_t telemetryAge = 0;
  uint32_t drivingAge = 0;
  const bool telemetryFresh = motBleClientLatest(telemetry, telemetryAge);
  const bool drivingFresh = motBleClientLatestDriving(driving, drivingAge);

  const uint32_t accent = telemetryFresh ? flowColor(telemetry) : kMuted;
  const int powerDeciKw = telemetryFresh ? abs(telemetry.powerDeciKw) : 0;
  lv_arc_set_value(powerArc, min(powerDeciKw, 250));
  lv_obj_set_style_arc_color(powerArc, lv_color_hex(accent), LV_PART_INDICATOR);

  if (telemetryFresh && (telemetry.flags & MotDisplayBle::kSocValid))
    lv_label_set_text_fmt(socLabel, "SOC %u%%", telemetry.socPercent);
  else lv_label_set_text(socLabel, "SOC --");

  if (telemetryFresh && (telemetry.flags & MotDisplayBle::kDataValid)) {
    const int32_t shownPowerDeciKw = (telemetry.flags & MotDisplayBle::kCharging)
        ? abs(telemetry.powerDeciKw) : telemetry.powerDeciKw;
    const String powerText = fixedTenths(shownPowerDeciKw, true) + " kW";
    lv_label_set_text(powerLabel, powerText.c_str());
  } else lv_label_set_text(powerLabel, "--.- kW");
  lv_obj_set_style_text_color(powerLabel, lv_color_hex(accent), 0);

  if (telemetryFresh && (telemetry.flags & MotDisplayBle::kRangeValid) &&
      telemetry.rangeToTargetKm >= 0) {
    const int shownRange = presentationUnits == MotDisplayBle::kUnitsUk
        ? static_cast<int>(lroundf(telemetry.rangeToTargetKm * kKmToMiles))
        : telemetry.rangeToTargetKm;
    lv_label_set_text_fmt(rangeLabel, "%d %s %s %u%%", shownRange,
                          presentationUnits == MotDisplayBle::kUnitsUk ? "mi" : "km",
                          textFor("bis", "to", "a", "a"), telemetry.targetSocPercent);
  } else {
    lv_label_set_text_fmt(rangeLabel, "-- %s %s --%%",
                          presentationUnits == MotDisplayBle::kUnitsUk ? "mi" : "km",
                          textFor("bis", "to", "a", "a"));
  }

  lv_label_set_text(stateLabel, telemetryFresh ? flowLabel(telemetry) : motBleClientStatus());
  lv_obj_set_style_text_color(stateLabel, lv_color_hex(accent), 0);

  const bool speedValid = drivingFresh && (driving.flags & MotDisplayBle::kDrivingSpeedValid);
  if (speedValid) {
    const float speedKmh = driving.speedDeciKmh / 10.0F;
    lv_label_set_text_fmt(speedLabel, "%u\n%s",
                          static_cast<unsigned>(lroundf(presentationUnits == MotDisplayBle::kUnitsUk
                              ? speedKmh * kKmToMiles : speedKmh)),
                          presentationUnits == MotDisplayBle::kUnitsUk ? "mph" : "km/h");
  } else {
    lv_label_set_text_fmt(speedLabel, "--\n%s",
                          presentationUnits == MotDisplayBle::kUnitsUk ? "mph" : "km/h");
  }
  updateSpeedWarning(speedValid, driving);

  const bool indicatorsValid = drivingFresh &&
      (driving.flags & MotDisplayBle::kIndicatorStateValid);
  lv_label_set_text(leftLabel, indicatorsValid &&
                    (driving.flags & MotDisplayBle::kLeftIndicatorActive) ? LV_SYMBOL_LEFT : "");
  lv_label_set_text(rightLabel, indicatorsValid &&
                    (driving.flags & MotDisplayBle::kRightIndicatorActive) ? LV_SYMBOL_RIGHT : "");

  lv_label_set_text(parkingLabel, drivingFresh &&
                    (driving.flags & MotDisplayBle::kSafetyStateValid) &&
                    (driving.flags & MotDisplayBle::kParkingBrakeActive) ? "(P)" : "");

  if (drivingFresh && (driving.flags & MotDisplayBle::kCandidateStatesValid)) {
    const uint8_t gear = (driving.flags & MotDisplayBle::kGearStateMask) >>
        MotDisplayBle::kGearStateShift;
    lv_label_set_text(gearLabel, gear == MotDisplayBle::kGearDrive ? "D" :
                      gear == MotDisplayBle::kGearReverse ? "R" :
                      gear == MotDisplayBle::kGearNeutral ? "N" : "");
    const uint8_t light = (driving.flags & MotDisplayBle::kLightStateMask) >>
        MotDisplayBle::kLightStateShift;
    lv_label_set_text(lightLabel, light == MotDisplayBle::kLightBit1 ? "LICHT 0" :
                      light == MotDisplayBle::kLightBit2 ? "LICHT 2" :
                      light == MotDisplayBle::kLightBits1And2 ? "LICHT 0+2" : "");
    const uint8_t mode = (driving.flags & MotDisplayBle::kDriveModeMask) >>
        MotDisplayBle::kDriveModeShift;
    lv_label_set_text(modeLabel, mode == MotDisplayBle::kDriveModeSport ? "SPORT" :
                      mode == MotDisplayBle::kDriveModeEco ? "ECO" : "");
  } else {
    lv_label_set_text(gearLabel, "");
    lv_label_set_text(lightLabel, "");
    lv_label_set_text(modeLabel, "");
  }

  String warnings;
  if (drivingFresh && (driving.flags & MotDisplayBle::kSafetyStateValid)) {
    if (driving.flags & MotDisplayBle::kWarning90Active) warnings += "! WARNUNG 90";
    if (driving.flags & MotDisplayBle::kWarningHblAhActive)
      warnings += warnings.length() ? "  ! HBL/AH" : "! HBL/AH";
    if (driving.flags & MotDisplayBle::kDrivetrainWarningYellow)
      warnings += warnings.length() ? "  ! ANTRIEB" : "! ANTRIEB";
  }
  lv_label_set_text(warningLabel, warnings.c_str());
}

void updateStatusPage() {
  lv_label_set_text(statusTitle,
      textFor("MOT STATUS", "MOT STATUS", "ETAT MOT", "STATO MOT"));
  lv_label_set_text(statusPairingHint,
                    textFor("3 s Touch/Taste: koppeln", "Hold touch/key 3 s: re-pair",
                            "Toucher/touche 3 s: recoupler", "Touch/tasto 3 s: riassocia"));

  if (static_cast<int32_t>(pairingResetNoticeUntilMs - millis()) > 0) {
    lv_obj_remove_flag(statusBody, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusWifi1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusWifi2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusNetwork, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusAws, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(statusBody,
                      textFor("ZUORDNUNG GELOESCHT\n\nSUCHE MOT ...",
                              "PAIRING CLEARED\n\nSEARCHING FOR MOT ...",
                              "ASSOCIATION EFFACEE\n\nRECHERCHE MOT ...",
                              "ASSOCIAZIONE CANCELLATA\n\nRICERCA MOT ..."));
    return;
  }
  MotDisplayBle::StatusV1 status{};
  uint32_t age = 0;
  if (!motBleClientLatestStatus(status, age)) {
    lv_obj_remove_flag(statusBody, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusWifi1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusWifi2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusNetwork, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusAws, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(statusBody, "%s\n\n%s",
                          motBleClientStatus(),
                          textFor("Tippen: Einstellungen", "Tap: settings",
                                  "Toucher: reglages", "Tocca: impostazioni"));
    return;
  }
  lv_obj_add_flag(statusBody, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(statusWifi1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(statusWifi2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(statusNetwork, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(statusAws, LV_OBJ_FLAG_HIDDEN);
  const bool aws = status.flags & MotDisplayBle::kAwsConnected;
  const bool wifi1Active = status.flags & MotDisplayBle::kWifi1Active;
  const bool wifi2Active = status.flags & MotDisplayBle::kWifi2Active;
  const bool wifi1Configured = status.flags & MotDisplayBle::kWifi1Configured;
  const bool wifi2Configured = status.flags & MotDisplayBle::kWifi2Configured;
  const char *inactive = textFor("BEREIT, NICHT AKTIV", "READY, NOT ACTIVE",
                                 "PRET, INACTIF", "PRONTO, NON ATTIVO");
  const char *unset = textFor("NICHT GESETZT", "NOT SET", "NON DEFINI", "NON IMPOSTATO");
  const String wifi1State = wifi1Active ? String("AKTIV ") + status.wifiRssi + " dBm" :
      (wifi1Configured ? inactive : unset);
  const String wifi2State = wifi2Active ? String("AKTIV ") + status.wifiRssi + " dBm" :
      (wifi2Configured ? inactive : unset);
  const String wifi1Block = String("WIFI 1: ") +
      (status.wifi1Name[0] ? status.wifi1Name : "--") + "\n" + wifi1State;
  const String wifi2Block = String("WIFI 2: ") +
      (status.wifi2Name[0] ? status.wifi2Name : "--") + "\n" + wifi2State;
  const String networkBlock = String("IP: ") +
      (status.ipAddress[0] ? status.ipAddress : "--") + "\nAP: " +
      ((status.flags2 & MotDisplayBle::kAccessPointActive) ?
          textFor("AKTIV", "ACTIVE", "ACTIF", "ATTIVO") :
          textFor("AUS", "OFF", "ARRETE", "SPENTO"));
  const String awsBlock = String("AWS: ") +
      (aws ? textFor("VERBUNDEN", "CONNECTED", "CONNECTE", "CONNESSO") :
             textFor("GETRENNT", "DISCONNECTED", "DECONNECTE", "DISCONNESSO"));
  lv_label_set_text(statusWifi1, wifi1Block.c_str());
  lv_label_set_text(statusWifi2, wifi2Block.c_str());
  lv_label_set_text(statusNetwork, networkBlock.c_str());
  lv_label_set_text(statusAws, awsBlock.c_str());
  lv_obj_set_style_text_color(statusWifi1, lv_color_hex(wifi1Active ? kRegen : kFg), 0);
  lv_obj_set_style_text_color(statusWifi2, lv_color_hex(wifi2Active ? kRegen : kFg), 0);
  lv_obj_set_style_text_color(statusAws, lv_color_hex(aws ? kRegen : kFg), 0);
}

void initLvgl() {
  lv_init();
  lv_tick_set_cb(lvTickMillis);
  display = lv_display_create(360, 360);
  lv_display_set_flush_cb(display, flushDisplay);
  // DMA-capable buffers avoid the SPI driver allocating a temporary internal
  // transfer buffer for every LVGL flush. Four lines per buffer keep enough
  // contiguous internal RAM available for the encrypted BLE client.
  constexpr size_t kBufferPixels = 360 * 4;
  drawBuffer1 = static_cast<lv_color_t *>(
      heap_caps_malloc(kBufferPixels * sizeof(lv_color_t),
                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  drawBuffer2 = static_cast<lv_color_t *>(
      heap_caps_malloc(kBufferPixels * sizeof(lv_color_t),
                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (!drawBuffer1 || !drawBuffer2) {
    Serial.println("FATAL: LVGL DMA buffers unavailable");
    while (true) delay(1000);
  }
  lv_display_set_buffers(display, drawBuffer1, drawBuffer2,
                         kBufferPixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  touch = lv_indev_create();
  lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch, readTouch);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n%s\n", kFirmwareVersion);
  Serial.printf("Flash %u MB, PSRAM %u MB\n",
                ESP.getFlashChipSize() / (1024U * 1024U),
                ESP.getPsramSize() / (1024U * 1024U));
  uiPreferences.begin("mot-ui", false);
  presentationLanguage = uiPreferences.getUChar("language", MotDisplayBle::kLanguageGerman);
  if (presentationLanguage > MotDisplayBle::kLanguageItalian)
    presentationLanguage = MotDisplayBle::kLanguageGerman;
  presentationUnits = uiPreferences.getUChar("units", MotDisplayBle::kUnitsMetric);
  if (presentationUnits > MotDisplayBle::kUnitsUk)
    presentationUnits = MotDisplayBle::kUnitsMetric;
  followAdapterConfig = uiPreferences.getBool("followMot", true);
  speedLimitValue = uiPreferences.getUChar("speedLimit", 0);
  if (speedLimitValue != 0 && speedLimitValue != 30 && speedLimitValue != 50 &&
      speedLimitValue != 60 && speedLimitValue != 80) speedLimitValue = 0;
  speedWarningMuted = uiPreferences.getBool("speedMute", false);
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Backlight_Init();
  LCD_Init();
  initLvgl();
  createPowerPage();
  createSpeedControlPage();
  createJourneyPage();
  createBatteryPage();
  createChargingPage();
  createVehicleBoardPage();
  createSoftwareVersionsPage();
  createStatusPage();
  createSettingsPage();
  createMotImagePage();
  loadPage(Page::Power);
  pinMode(kSideButtonPin, INPUT_PULLUP);
  audioReady = initSpeedWarningAudio();
  Serial.printf("Speed warning audio: %s\n", audioReady ? "ready" : "unavailable; visual only");
  motBleClientSetup();
  Serial.println("S3 1.85 touch display ready");
}

void loop() {
  motBleClientLoop();
  pollSideButton();
  lv_timer_handler();
  if (millis() - lastRefreshMs >= kRefreshMs) {
    lastRefreshMs = millis();
    syncPresentationConfig();
    updatePowerPage();
    updateSpeedControlPage();
    updateJourneyPage();
    updateBatteryPage();
    updateChargingPage();
    updateVehicleBoardPage();
    updateSoftwareVersionsPage();
    updateStatusPage();
    updateSettingsPage();

    MotDisplayBle::TelemetryV1 telemetry{};
    uint32_t telemetryAgeMs = 0;
    if (motBleClientLatest(telemetry, telemetryAgeMs) &&
        (telemetry.flags & MotDisplayBle::kDataValid)) {
      const bool nowCharging = telemetry.flags & MotDisplayBle::kCharging;
      if (nowCharging != chargingHome) {
        chargingHome = nowCharging;
        loadPage(homePage());
      }
    }

    const uint32_t returnMs = currentPage == Page::Status
        ? kStatusPageReturnMs : kPageReturnMs;
    if (currentPage != homePage() && millis() - lastPageSelectionMs >= returnMs) {
      loadPage(homePage());
    }
  }
  delay(5);
}
