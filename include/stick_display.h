#pragma once

#include <Adafruit_NeoPixel.h>
#include <shiftlight_config.h>

namespace Shiftlight {

class StickDisplay {
 public:
  StickDisplay()
      : pixels_(ShiftlightConfig::kLedCount, ShiftlightConfig::kLedPin,
                NEO_GRB + NEO_KHZ800) {}

  void begin() {
    pixels_.begin();
    pixels_.setBrightness(ShiftlightConfig::kBrightness);
    clear();
  }

  void clear() {
    pixels_.clear();
    pixels_.show();
  }

  void showRpm(uint16_t rpm) {
    for (uint8_t led = 0; led < ShiftlightConfig::kLedCount; ++led) {
      const uint32_t color = rpm >= ShiftlightConfig::kAllRedRpm
                                 ? 0xFF0000
                                 : rpm >= ShiftlightConfig::kLedThresholdRpm[led]
                                       ? ShiftlightConfig::kLedColors[led]
                                       : 0;
      // Порог 4300 RPM начинается с дальнего от входного разъёма конца стика.
      pixels_.setPixelColor(ShiftlightConfig::kLedCount - 1 - led, color);
    }
    pixels_.show();
  }

  void showStartupPair(uint8_t offset) {
    pixels_.clear();
    pixels_.setPixelColor(offset, ShiftlightConfig::kStartupOrange);
    pixels_.setPixelColor(ShiftlightConfig::kLedCount - 1 - offset,
                          ShiftlightConfig::kStartupOrange);
    pixels_.show();
  }

 private:
  Adafruit_NeoPixel pixels_;
};

}  // namespace Shiftlight
