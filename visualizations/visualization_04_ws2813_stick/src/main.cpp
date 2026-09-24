#include <Adafruit_NeoPixel.h>
#include <M5Unified.h>

#include "../../common/demo_rpm.h"

namespace {

constexpr uint8_t kLedPin = 1;
constexpr uint8_t kLedCount = 10;
constexpr uint8_t kBrightness = 32;
constexpr uint16_t kAllRedRpm = 6300;

constexpr uint16_t kThresholdRpm[kLedCount] = {
    4300, 4500, 4700, 4900, 5100, 5300, 5500, 5700, 5900, 6100,
};
constexpr uint32_t kLedColors[kLedCount] = {
    0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00,
    0xFFFF00, 0xFFFF00, 0xFFFF00, 0xFF0000, 0xFF0000,
};

Adafruit_NeoPixel strip(kLedCount, kLedPin, NEO_GRB + NEO_KHZ800);
uint16_t rpm = VisualizationDemo::kRpm.minimum;
int8_t rpmDirection = 1;
uint8_t peakHoldSteps = 0;
uint32_t lastRpmStepMs = 0;

void renderStick() {
  if (rpm >= kAllRedRpm) {
    for (uint8_t led = 0; led < kLedCount; ++led) {
      strip.setPixelColor(led, 0xFF0000);
    }
  } else {
    for (uint8_t led = 0; led < kLedCount; ++led) {
      strip.setPixelColor(led, rpm >= kThresholdRpm[led] ? kLedColors[led] : 0);
    }
  }
  strip.show();
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

  M5.Display.setTextColor(rpm >= kThresholdRpm[8] ? TFT_RED
                          : rpm >= kThresholdRpm[5] ? TFT_YELLOW : TFT_GREEN,
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

  strip.begin();
  strip.setBrightness(kBrightness);
  strip.clear();
  strip.show();

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
