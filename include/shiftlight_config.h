#pragma once

#include <stdint.h>

namespace ShiftlightConfig {

constexpr uint8_t kLedPin = 1;  // Белый провод SIG стика в Grove-порту AtomS3R.
constexpr uint8_t kLedCount = 10;
constexpr uint8_t kBrightness = 32;
constexpr uint16_t kLedThresholdRpm[kLedCount] = {
    4300, 4500, 4700, 4900, 5100, 5300, 5500, 5700, 5900, 6100,
};
constexpr uint32_t kLedColors[kLedCount] = {
    0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00,
    0xFFFF00, 0xFFFF00, 0xFFFF00, 0xFF0000, 0xFF0000,
};
constexpr uint16_t kAllRedRpm = 6300;

constexpr bool kShowEngineStartAnimation = true;
constexpr uint32_t kStartupOrange = 0xC03800;
constexpr uint16_t kStartupStepMs = 110;

// Предварительные границы для определения запуска по RPM; уточняются на машине.
constexpr uint16_t kStoppedRpm = 100;
constexpr uint16_t kRunningRpm = 400;
constexpr uint32_t kStoppedConfirmMs = 1000;
constexpr uint32_t kCrankingConfirmMs = 100;
constexpr uint32_t kRunningConfirmMs = 150;
constexpr uint32_t kRpmMissingResetMs = 2000;
constexpr uint32_t kRpmTimeoutMs = 200;

inline uint8_t litLedCount(uint16_t rpm) {
  if (rpm >= kAllRedRpm) return kLedCount;
  uint8_t count = 0;
  while (count < kLedCount && rpm >= kLedThresholdRpm[count]) ++count;
  return count;
}

}  // namespace ShiftlightConfig
