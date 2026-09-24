#include <M5Unified.h>
#include <engine_start_detector.h>
#include <shiftlight_config.h>
#include <stick_display.h>
#include "driver/twai.h"

namespace {

constexpr gpio_num_t kCanTxPin = GPIO_NUM_5;
constexpr gpio_num_t kCanRxPin = GPIO_NUM_6;
constexpr uint32_t kCanBitRate = 500000;
constexpr uint32_t kRpmCanId = 0x0A5;
constexpr uint8_t kRpmLowByte = 5;
constexpr uint8_t kRpmHighByte = 6;
constexpr uint32_t kScreenRefreshMs = 100;
constexpr uint8_t kAllRedMode = ShiftlightConfig::kLedCount + 1;
constexpr uint8_t kStartupFrameCount = ShiftlightConfig::kLedCount - 1;

Shiftlight::StickDisplay stick;
Shiftlight::EngineStartDetector engineStart;
uint16_t currentRpm = 0;
uint32_t lastRpmFrameMs = 0;
uint32_t lastScreenRefreshMs = 0;
uint32_t startupFrameStartedMs = 0;
uint8_t startupFrame = 0;
uint8_t lastRenderedMode = 0xFF;
bool haveRpmFrame = false;
bool startupActive = false;

void showFatal(const char* title, const char* detail) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString(title, M5.Display.width() / 2, 46);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(detail, M5.Display.width() / 2, 76);
}

bool initialiseCan() {
  twai_general_config_t general =
      TWAI_GENERAL_CONFIG_DEFAULT(kCanTxPin, kCanRxPin, TWAI_MODE_LISTEN_ONLY);
  twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  const esp_err_t installResult = twai_driver_install(&general, &timing, &filter);
  if (installResult != ESP_OK) {
    Serial.printf("TWAI install failed: %s\n", esp_err_to_name(installResult));
    return false;
  }
  const esp_err_t startResult = twai_start();
  if (startResult != ESP_OK) {
    Serial.printf("TWAI start failed: %s\n", esp_err_to_name(startResult));
    twai_driver_uninstall();
    return false;
  }

  Serial.printf("PT-CAN ready: %lu bit/s, listen-only, RPM frame 0x%03lX\n",
                static_cast<unsigned long>(kCanBitRate), static_cast<unsigned long>(kRpmCanId));
  return true;
}

bool readRpmFrames() {
  bool updated = false;
  twai_message_t message = {};
  while (twai_receive(&message, 0) == ESP_OK) {
    if (message.extd || message.identifier != kRpmCanId ||
        message.data_length_code <= kRpmHighByte) {
      continue;
    }
    const uint16_t raw = static_cast<uint16_t>(message.data[kRpmLowByte]) |
                         (static_cast<uint16_t>(message.data[kRpmHighByte]) << 8);
    currentRpm = (raw + 2) / 4;
    lastRpmFrameMs = millis();
    haveRpmFrame = true;
    updated = true;
  }
  return updated;
}

void renderScreen(bool rpmFresh) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  if (!rpmFresh) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.drawString("WAITING", M5.Display.width() / 2, 36);
    M5.Display.drawString("FOR CAN", M5.Display.width() / 2, 88);
    return;
  }

  char rpmText[8] = {};
  snprintf(rpmText, sizeof(rpmText), "%u", currentRpm);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextSize(M5.Display.width() / M5.Display.textWidth(rpmText));
  M5.Display.drawString(rpmText, M5.Display.width() / 2, 45);

  M5.Display.setTextColor(currentRpm >= ShiftlightConfig::kLedThresholdRpm[8] ? TFT_RED
                          : currentRpm >= ShiftlightConfig::kLedThresholdRpm[5] ? TFT_YELLOW
                                                                                : TFT_GREEN,
                          TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("RPM", M5.Display.width() / 2, 79);
  char statusText[16] = {};
  if (currentRpm >= ShiftlightConfig::kAllRedRpm) {
    snprintf(statusText, sizeof(statusText), "ALL RED");
  } else {
    snprintf(statusText, sizeof(statusText), "LED %u/10",
             ShiftlightConfig::litLedCount(currentRpm));
  }
  M5.Display.drawString(statusText, M5.Display.width() / 2, 104);
}

void startAnimation(uint32_t now) {
  startupActive = true;
  startupFrame = 0;
  startupFrameStartedMs = now;
  lastRenderedMode = 0xFF;
  stick.showStartupPair(0);
  Serial.println("Engine start: orange panel sweep");
}

void advanceAnimation(uint32_t now) {
  if (!startupActive ||
      now - startupFrameStartedMs < ShiftlightConfig::kStartupStepMs) return;

  startupFrameStartedMs += ShiftlightConfig::kStartupStepMs;
  ++startupFrame;
  if (startupFrame >= kStartupFrameCount) {
    startupActive = false;
    return;
  }
  const uint8_t offset = startupFrame < ShiftlightConfig::kLedCount / 2
                             ? startupFrame
                             : kStartupFrameCount - 1 - startupFrame;
  stick.showStartupPair(offset);
}

void updateStick(bool rpmFresh, uint32_t now) {
  if (startupActive &&
      (!rpmFresh || currentRpm >= ShiftlightConfig::kLedThresholdRpm[0])) {
    startupActive = false;
  }
  advanceAnimation(now);
  if (startupActive) return;

  const uint8_t mode = !rpmFresh ? 0
                       : currentRpm >= ShiftlightConfig::kAllRedRpm
                           ? kAllRedMode
                           : ShiftlightConfig::litLedCount(currentRpm);
  if (mode == lastRenderedMode) return;
  if (rpmFresh) {
    stick.showRpm(currentRpm);
  } else {
    stick.clear();
  }
  lastRenderedMode = mode;
}

}  // namespace

void setup() {
  auto config = M5.config();
  M5.begin(config);
  M5.Display.setRotation(3);
  Serial.begin(115200);
  delay(200);

  stick.begin();
  if (!initialiseCan()) {
    showFatal("CAN INIT FAILED", "Atomic CAN Base / pins");
    while (true) delay(250);
  }

  renderScreen(false);
  lastScreenRefreshMs = millis();
  Serial.println("Shiftlight ready; waiting for PT-CAN RPM frame 0x0A5.");
}

void loop() {
  M5.update();
  readRpmFrames();
  const uint32_t now = millis();
  const bool rpmFresh = haveRpmFrame &&
                        now - lastRpmFrameMs <= ShiftlightConfig::kRpmTimeoutMs;

  if (ShiftlightConfig::kShowEngineStartAnimation &&
      engineStart.update(rpmFresh, currentRpm, now)) {
    startAnimation(now);
  }
  updateStick(rpmFresh, now);

  if (now - lastScreenRefreshMs >= kScreenRefreshMs) {
    renderScreen(rpmFresh);
    lastScreenRefreshMs = now;
  }
}
