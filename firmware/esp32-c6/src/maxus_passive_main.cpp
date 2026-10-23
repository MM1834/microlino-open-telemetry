#include <Arduino.h>
#include <driver/twai.h>

#include <algorithm>
#include <cstring>

#ifndef MOT_MAXUS_PASSIVE_DIAGNOSTIC
#error "This source is only for the isolated Maxus passive diagnostic build"
#endif

namespace {

constexpr uint32_t kDefaultBaud = 500000;
constexpr size_t kMaxIdentifiers = 192;
constexpr uint8_t kMaxDlc = 8;
constexpr uint32_t kStatusIntervalMs = 5000;
constexpr uint32_t kArmWindowMs = 30000;
constexpr uint32_t kUdsTimeoutMs = 2500;
constexpr size_t kMaxUdsPayload = 512;

struct UdsTarget {
  const char *name;
  uint32_t requestId;
  uint32_t responseId;
};

constexpr UdsTarget kUdsTargets[] = {
    {"vcu", 0x7E3, 0x7EB},
    {"bms", 0x748, 0x7C8},
};

constexpr uint16_t kAllowedDids[] = {
    0xB105, 0xB110, 0xB11E, 0xD117,
    0xE002, 0xE003, 0xE005, 0xE006, 0xE015, 0xE018, 0xE019, 0xE020,
    0xE022, 0xE025, 0xE037, 0xE053, 0xE088, 0xE089, 0xE095, 0xE096,
    0xE113, 0xE114, 0xE131, 0xF190,
};

struct IdentifierState {
  bool used = false;
  bool extended = false;
  uint32_t id = 0;
  uint32_t frames = 0;
  uint32_t firstMs = 0;
  uint32_t lastMs = 0;
  uint8_t dlc = 0;
  uint8_t first[kMaxDlc] = {};
  uint8_t last[kMaxDlc] = {};
  uint8_t minimum[kMaxDlc] = {};
  uint8_t maximum[kMaxDlc] = {};
  uint8_t changedMask = 0;
};

twai_handle_t canHandle = nullptr;
IdentifierState identifiers[kMaxIdentifiers];
uint32_t totalFrames = 0;
uint32_t receiveErrors = 0;
uint32_t droppedIdentifiers = 0;
uint32_t lastFrameMs = 0;
uint32_t lastStatusMs = 0;
bool rawStream = false;
bool udsEnabled = false;
uint32_t currentBaud = kDefaultBaud;
uint32_t udsArmDeadlineMs = 0;
String serialCommand;

void printBytes(const uint8_t *data, uint8_t length)
{
  for (uint8_t index = 0; index < length; ++index) {
    Serial.printf("%02X%s", data[index], index + 1 < length ? " " : "");
  }
}

IdentifierState *findOrAllocate(uint32_t id, bool extended)
{
  IdentifierState *freeSlot = nullptr;
  for (auto &state : identifiers) {
    if (state.used && state.id == id && state.extended == extended) return &state;
    if (!state.used && freeSlot == nullptr) freeSlot = &state;
  }
  if (freeSlot != nullptr) {
    freeSlot->used = true;
    freeSlot->id = id;
    freeSlot->extended = extended;
  }
  return freeSlot;
}

void resetCapture()
{
  memset(identifiers, 0, sizeof(identifiers));
  totalFrames = 0;
  receiveErrors = 0;
  droppedIdentifiers = 0;
  lastFrameMs = 0;
  Serial.println("MAXUS capture reset");
}

size_t identifierCount()
{
  size_t count = 0;
  for (const auto &state : identifiers) if (state.used) ++count;
  return count;
}

void printStatus()
{
  Serial.printf("MAXUS status mode=%s bus=CAN1 bitrate=%lu tx=%s frames=%lu ids=%u rxErrors=%lu droppedIds=%lu lastFrameAgeMs=",
                udsEnabled ? "uds-read-only" : "passive",
                static_cast<unsigned long>(currentBaud),
                udsEnabled ? "whitelist-only" : "disabled",
                static_cast<unsigned long>(totalFrames),
                static_cast<unsigned>(identifierCount()),
                static_cast<unsigned long>(receiveErrors),
                static_cast<unsigned long>(droppedIdentifiers));
  if (totalFrames) Serial.println(static_cast<unsigned long>(millis() - lastFrameMs));
  else Serial.println("never");
}

bool setupCan(bool active);
void observe(const twai_message_t &message);

bool setPassiveBitrate(uint32_t baud)
{
  if (udsEnabled) {
    Serial.println("MAXUS bitrate change rejected: return to passive mode first");
    return false;
  }
  if (baud != 125000 && baud != 250000 && baud != 500000 && baud != 1000000) {
    Serial.println("MAXUS bitrate rejected: allowed values are 125000, 250000, 500000, 1000000");
    return false;
  }
  if (canHandle != nullptr) {
    twai_stop_v2(canHandle);
    twai_driver_uninstall_v2(canHandle);
    canHandle = nullptr;
  }
  currentBaud = baud;
  resetCapture();
  if (!setupCan(false)) {
    Serial.println("MAXUS bitrate change failed; recovering at 500000");
    currentBaud = kDefaultBaud;
    return setupCan(false);
  }
  Serial.printf("MAXUS passive bitrate=%lu tx=disabled\n", static_cast<unsigned long>(currentBaud));
  return true;
}

bool setUdsMode(bool enabled)
{
  if (canHandle != nullptr) {
    twai_stop_v2(canHandle);
    twai_driver_uninstall_v2(canHandle);
    canHandle = nullptr;
  }
  udsEnabled = enabled;
  if (!setupCan(enabled)) {
    udsEnabled = false;
    Serial.println("MAXUS CAN mode change failed; attempting passive recovery");
    return setupCan(false);
  }
  Serial.printf("MAXUS mode=%s tx=%s\n", enabled ? "uds-read-only" : "passive",
                enabled ? "whitelist-only" : "disabled");
  return true;
}

const UdsTarget *findTarget(const String &name)
{
  for (const auto &target : kUdsTargets) if (name == target.name) return &target;
  return nullptr;
}

bool didAllowed(uint16_t did)
{
  for (uint16_t allowed : kAllowedDids) if (did == allowed) return true;
  return false;
}

bool transmit(uint32_t id, const uint8_t *data, uint8_t length)
{
  if (!udsEnabled || canHandle == nullptr || length > 8) return false;
  twai_message_t message{};
  message.identifier = id;
  message.data_length_code = length;
  memcpy(message.data, data, length);
  const esp_err_t result = twai_transmit_v2(canHandle, &message, pdMS_TO_TICKS(100));
  Serial.printf("MAXUS TX id=0x%03lX dlc=%u result=%s data=",
                static_cast<unsigned long>(id), static_cast<unsigned>(length),
                esp_err_to_name(result));
  printBytes(data, length);
  Serial.println();
  return result == ESP_OK;
}

void udsRead(const UdsTarget &target, uint16_t did)
{
  if (!udsEnabled) {
    Serial.println("MAXUS UDS rejected: enable read-only mode first");
    return;
  }
  if (!didAllowed(did)) {
    Serial.printf("MAXUS UDS rejected: DID 0x%04X is not whitelisted\n", did);
    return;
  }

  const uint8_t request[8] = {0x03, 0x22, static_cast<uint8_t>(did >> 8),
                              static_cast<uint8_t>(did), 0, 0, 0, 0};
  if (!transmit(target.requestId, request, sizeof(request))) return;

  uint8_t payload[kMaxUdsPayload] = {};
  size_t expected = 0;
  size_t received = 0;
  uint8_t nextSequence = 1;
  const uint32_t started = millis();
  while (millis() - started < kUdsTimeoutMs) {
    twai_message_t message{};
    const esp_err_t result = twai_receive_v2(canHandle, &message, pdMS_TO_TICKS(20));
    if (result == ESP_ERR_TIMEOUT) continue;
    if (result != ESP_OK) { ++receiveErrors; continue; }
    observe(message);
    if (message.extd || message.identifier != target.responseId || message.data_length_code < 2) continue;

    const uint8_t type = message.data[0] >> 4;
    if (type == 0x0) {
      expected = message.data[0] & 0x0F;
      if (expected > 7) break;
      memcpy(payload, &message.data[1], expected);
      received = expected;
      break;
    }
    if (type == 0x1) {
      expected = (static_cast<size_t>(message.data[0] & 0x0F) << 8) | message.data[1];
      if (expected == 0 || expected > kMaxUdsPayload) {
        Serial.printf("MAXUS UDS rejected response length=%u\n", static_cast<unsigned>(expected));
        return;
      }
      const size_t initial = min<size_t>(expected, message.data_length_code - 2);
      memcpy(payload, &message.data[2], initial);
      received = initial;
      const uint8_t flowControl[8] = {0x30, 0x00, 0x0A, 0, 0, 0, 0, 0};
      if (!transmit(target.requestId, flowControl, sizeof(flowControl))) return;
      continue;
    }
    if (type == 0x2 && expected > 0) {
      const uint8_t sequence = message.data[0] & 0x0F;
      if (sequence != (nextSequence & 0x0F)) {
        Serial.printf("MAXUS UDS sequence error expected=%u received=%u\n",
                      nextSequence & 0x0F, sequence);
        return;
      }
      ++nextSequence;
      const size_t chunk = min<size_t>(expected - received, message.data_length_code - 1);
      memcpy(&payload[received], &message.data[1], chunk);
      received += chunk;
      if (received >= expected) break;
    }
  }

  if (received < expected || received == 0) {
    Serial.printf("MAXUS UDS timeout target=%s did=0x%04X received=%u expected=%u\n",
                  target.name, did, static_cast<unsigned>(received), static_cast<unsigned>(expected));
    return;
  }
  Serial.printf("MAXUS UDS response target=%s did=0x%04X length=%u data=",
                target.name, did, static_cast<unsigned>(received));
  printBytes(payload, static_cast<uint8_t>(min<size_t>(received, 255)));
  Serial.println();
  if (received >= 3 && payload[0] == 0x62 &&
      payload[1] == static_cast<uint8_t>(did >> 8) && payload[2] == static_cast<uint8_t>(did)) {
    Serial.println("MAXUS UDS result=positive-read-response");
  } else if (received >= 3 && payload[0] == 0x7F) {
    Serial.printf("MAXUS UDS result=negative-response service=0x%02X nrc=0x%02X\n",
                  payload[1], payload[2]);
  } else {
    Serial.println("MAXUS UDS result=unexpected-payload");
  }
}

void printIdentifier(const IdentifierState &state)
{
  Serial.printf("MAXUS ID %s 0x%0*lX frames=%lu firstMs=%lu lastMs=%lu dlc=%u changed=0x%02X first=",
                state.extended ? "EXT" : "STD", state.extended ? 8 : 3,
                static_cast<unsigned long>(state.id),
                static_cast<unsigned long>(state.frames),
                static_cast<unsigned long>(state.firstMs),
                static_cast<unsigned long>(state.lastMs),
                static_cast<unsigned>(state.dlc), state.changedMask);
  printBytes(state.first, state.dlc);
  Serial.print(" last=");
  printBytes(state.last, state.dlc);
  Serial.print(" min=");
  printBytes(state.minimum, state.dlc);
  Serial.print(" max=");
  printBytes(state.maximum, state.dlc);
  Serial.println();
}

void dumpCapture(bool summaryOnly)
{
  Serial.printf("MAXUS %s begin ids=%u frames=%lu\n", summaryOnly ? "ids" : "dump",
                static_cast<unsigned>(identifierCount()),
                static_cast<unsigned long>(totalFrames));

  IdentifierState *ordered[kMaxIdentifiers];
  size_t count = 0;
  for (auto &state : identifiers) if (state.used) ordered[count++] = &state;
  std::sort(ordered, ordered + count, [](const IdentifierState *left, const IdentifierState *right) {
    if (left->extended != right->extended) return !left->extended;
    return left->id < right->id;
  });

  for (size_t index = 0; index < count; ++index) {
    if (summaryOnly) {
      Serial.printf("MAXUS ID %s 0x%0*lX frames=%lu dlc=%u changed=0x%02X\n",
                    ordered[index]->extended ? "EXT" : "STD",
                    ordered[index]->extended ? 8 : 3,
                    static_cast<unsigned long>(ordered[index]->id),
                    static_cast<unsigned long>(ordered[index]->frames),
                    static_cast<unsigned>(ordered[index]->dlc),
                    ordered[index]->changedMask);
    } else {
      printIdentifier(*ordered[index]);
    }
  }
  Serial.printf("MAXUS %s end droppedIds=%lu rxErrors=%lu\n",
                summaryOnly ? "ids" : "dump",
                static_cast<unsigned long>(droppedIdentifiers),
                static_cast<unsigned long>(receiveErrors));
}

void observe(const twai_message_t &message)
{
  const uint32_t now = millis();
  ++totalFrames;
  lastFrameMs = now;

  IdentifierState *state = findOrAllocate(message.identifier, message.extd);
  if (state == nullptr) {
    ++droppedIdentifiers;
  } else {
    const uint8_t length = min<uint8_t>(message.data_length_code, kMaxDlc);
    if (state->frames == 0) {
      state->firstMs = now;
      state->dlc = length;
      memcpy(state->first, message.data, length);
      memcpy(state->last, message.data, length);
      memcpy(state->minimum, message.data, length);
      memcpy(state->maximum, message.data, length);
    } else {
      if (state->dlc != length) state->changedMask = 0xFF;
      for (uint8_t index = 0; index < length; ++index) {
        if (state->last[index] != message.data[index]) {
          state->changedMask |= static_cast<uint8_t>(1U << index);
        }
        state->minimum[index] = min(state->minimum[index], message.data[index]);
        state->maximum[index] = max(state->maximum[index], message.data[index]);
        state->last[index] = message.data[index];
      }
      state->dlc = length;
    }
    ++state->frames;
    state->lastMs = now;
  }

  if (rawStream) {
    Serial.printf("MAXUS RAW %lu %s 0x%0*lX %u ",
                  static_cast<unsigned long>(now), message.extd ? "EXT" : "STD",
                  message.extd ? 8 : 3, static_cast<unsigned long>(message.identifier),
                  static_cast<unsigned>(message.data_length_code));
    printBytes(message.data, min<uint8_t>(message.data_length_code, kMaxDlc));
    Serial.println();
  }
}

void handleCommand(String command)
{
  command.trim();
  command.toLowerCase();
  if (command == "status") printStatus();
  else if (command == "reset") resetCapture();
  else if (command == "ids") dumpCapture(true);
  else if (command == "dump") dumpCapture(false);
  else if (command == "stream on") { rawStream = true; Serial.println("MAXUS raw stream enabled"); }
  else if (command == "stream off") { rawStream = false; Serial.println("MAXUS raw stream disabled"); }
  else if (command.startsWith("mark ")) Serial.println("MAXUS MARK " + String(millis()) + " " + command.substring(5));
  else if (command.startsWith("bitrate ")) {
    const String baudText = command.substring(8);
    char *end = nullptr;
    const unsigned long baud = strtoul(baudText.c_str(), &end, 10);
    if (end == baudText.c_str() || *end != '\0') {
      Serial.println("Usage: bitrate <125000|250000|500000|1000000>");
    } else {
      setPassiveBitrate(static_cast<uint32_t>(baud));
    }
  }
  else if (command == "uds arm read-only") {
    udsArmDeadlineMs = millis() + kArmWindowMs;
    Serial.println("MAXUS UDS armed for 30 seconds; enter: uds enable");
  }
  else if (command == "uds enable") {
    if (udsArmDeadlineMs == 0 || static_cast<int32_t>(udsArmDeadlineMs - millis()) <= 0) {
      Serial.println("MAXUS UDS enable rejected: arm window absent or expired");
    } else {
      udsArmDeadlineMs = 0;
      setUdsMode(true);
    }
  }
  else if (command == "uds passive") {
    udsArmDeadlineMs = 0;
    setUdsMode(false);
  }
  else if (command.startsWith("uds read ")) {
    const int separator = command.indexOf(' ', 9);
    if (separator < 0) {
      Serial.println("Usage: uds read <vcu|bms> <whitelisted-did>");
    } else {
      const UdsTarget *target = findTarget(command.substring(9, separator));
      const String didText = command.substring(separator + 1);
      char *end = nullptr;
      const unsigned long parsed = strtoul(didText.c_str(), &end, 16);
      if (target == nullptr || end == didText.c_str() || *end != '\0' || parsed > 0xFFFF) {
        Serial.println("Usage: uds read <vcu|bms> <whitelisted-did>");
      } else {
        udsRead(*target, static_cast<uint16_t>(parsed));
      }
    }
  }
  else if (command == "uds list") {
    Serial.println("MAXUS UDS targets: vcu=0x7E3/0x7EB bms=0x748/0x7C8; service=0x22 only");
    Serial.print("MAXUS UDS DIDs:");
    for (uint16_t did : kAllowedDids) Serial.printf(" %04X", did);
    Serial.println();
  }
  else Serial.println("Commands: status | reset | ids | dump | stream on | stream off | mark <label> | bitrate <125000|250000|500000|1000000> | uds arm read-only | uds enable | uds list | uds read <vcu|bms> <did> | uds passive");
}

void pollSerial()
{
  while (Serial.available()) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\n' || character == '\r') {
      if (!serialCommand.isEmpty()) {
        handleCommand(serialCommand);
        serialCommand = "";
      }
    } else if (serialCommand.length() < 96) {
      serialCommand += character;
    }
  }
}

bool setupCan(bool active)
{
  twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(MOT_CAN1_TX_PIN),
      static_cast<gpio_num_t>(MOT_CAN1_RX_PIN), active ? TWAI_MODE_NORMAL : TWAI_MODE_LISTEN_ONLY);
  general.controller_id = 0;
  general.tx_queue_len = active ? 8 : 0;
  general.rx_queue_len = 64;
  twai_timing_config_t timing{};
  switch (currentBaud) {
    case 125000: timing = TWAI_TIMING_CONFIG_125KBITS(); break;
    case 250000: timing = TWAI_TIMING_CONFIG_250KBITS(); break;
    case 1000000: timing = TWAI_TIMING_CONFIG_1MBITS(); break;
    case 500000:
    default: timing = TWAI_TIMING_CONFIG_500KBITS(); break;
  }
  const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t result = twai_driver_install_v2(&general, &timing, &filter, &canHandle);
  if (result != ESP_OK) {
    Serial.printf("MAXUS CAN install failed: %s\n", esp_err_to_name(result));
    return false;
  }
  result = twai_start_v2(canHandle);
  if (result != ESP_OK) {
    Serial.printf("MAXUS CAN start failed: %s\n", esp_err_to_name(result));
    return false;
  }
  return true;
}

}  // namespace

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("MOT MAXUS DIAGNOSTIC 0.2");
  Serial.printf("Board=%s CAN1 RX=%d TX-pin-unused=%d bitrate=%lu\n",
                MOT_BOARD, MOT_CAN1_RX_PIN, MOT_CAN1_TX_PIN,
                static_cast<unsigned long>(currentBaud));
  Serial.println("SAFETY: boots listen-only; UDS TX requires two-step CLI enable; no WiFi/cloud/MQTT/decoder");
  Serial.println("Expected hardware: CAN1 TX deliberately connected for later whitelisted UDS read tests");
  resetCapture();
  if (!setupCan(false)) {
    Serial.println("MAXUS CAN1 unavailable; do not continue vehicle test");
  } else {
    Serial.println("MAXUS CAN1 passive ready; enter any unknown command for help");
  }
}

void loop()
{
  if (canHandle != nullptr) {
    twai_message_t message{};
    const esp_err_t result = twai_receive_v2(canHandle, &message, 0);
    if (result == ESP_OK) observe(message);
    else if (result != ESP_ERR_TIMEOUT) ++receiveErrors;
  }
  pollSerial();
  if (millis() - lastStatusMs >= kStatusIntervalMs) {
    lastStatusMs = millis();
    printStatus();
  }
}
