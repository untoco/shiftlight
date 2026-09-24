#include <M5Unified.h>
#include <shiftlight_config.h>
#include <stick_display.h>

#include "../../common/demo_rpm.h"

namespace {

Shiftlight::StickDisplay stick;
uint16_t rpm = VisualizationDemo::kRpm.minimum;
int8_t rpmDirection = 1;
uint8_t peakHoldSteps = 0;
uint32_t lastRpmStepMs = 0;

void showStartupPair(uint8_t offset) {
  stick.showStartupPair(offset);
  delay(ShiftlightConfig::kStartupStepMs);
}

void runStartupTest() {
  for (uint8_t offset = 0; offset < ShiftlightConfig::kLedCount / 2; ++offset) {
    showStartupPair(offset);
  }
  for (int8_t offset = ShiftlightConfig::kLedCount / 2 - 2; offset >= 0; --offset) {
    showStartupPair(offset);
  }
  stick.clear();
}

void renderStick() {
  stick.showRpm(rpm);
}

void renderScreen() {
  char rpmText[8] = {};
  snprintf(rpmText, sizeof(rpmText), "%u", rpm);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextSize(M5.Display.width() / M5.Display.textWidth(rpmText));
  M5.Display.drawString(rpmText, M5.Display.width() / 2, 45);

  M5.Display.setTextColor(rpm >= ShiftlightConfig::kLedThresholdRpm[8] ? TFT_RED
                          : rpm >= ShiftlightConfig::kLedThresholdRpm[5] ? TFT_YELLOW
                                                                           : TFT_GREEN,
                          TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("RPM", M5.Display.width() / 2, 79);
  M5.Display.drawString("WS2813 STICK", M5.Display.width() / 2, 104);
}

void updateVisualisation() {
  renderStick();
  renderScreen();
  Serial.printf("RPM=%u\n", rpm);
}

void advanceRpm() {
  if (rpm == VisualizationDemo::kRpm.maximum) {
    if (peakHoldSteps++ < VisualizationDemo::kRpm.peakHoldSteps) return;
    peakHoldSteps = 0;
    rpmDirection = -1;
  } else if (rpm == VisualizationDemo::kRpm.minimum) {
    rpmDirection = 1;
  }
  rpm += rpmDirection * VisualizationDemo::kRpm.step;
}

}  // namespace

void setup() {
  auto config = M5.config();
  M5.begin(config);
  Serial.begin(115200);
  delay(200);

  stick.begin();
  runStartupTest();

  lastRpmStepMs = millis();
  updateVisualisation();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastRpmStepMs < VisualizationDemo::kRpm.stepIntervalMs) return;

  advanceRpm();
  updateVisualisation();
  lastRpmStepMs = now;
}
