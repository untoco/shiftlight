#include "startup_animation.h"

#include <M5Chain.h>

namespace StartupAnimation {
namespace {

constexpr uint8_t kMatrixWidth = 8;
constexpr uint8_t kMatrixHeight = 8;
constexpr uint8_t kFlagStartY = 4;
constexpr uint8_t kCheckerSize = 2;
constexpr uint8_t kSupportedMatrixCount = 3;
constexpr uint8_t kGroupsPerMatrix = kMatrixWidth / kCheckerSize;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint8_t kBrightnessLevels[] = {0, 35, 70, 100, 70, 35, 0};

}  // namespace

void addCheckerSquare(RGBPixelInfo (&pixels)[kGroupsPerMatrix * kCheckerSize * kCheckerSize],
                      uint8_t& pixelCount, uint8_t checkerX, uint8_t checkerY,
                      uint16_t color) {
  for (uint8_t y = 0; y < kCheckerSize; ++y) {
    for (uint8_t x = 0; x < kCheckerSize; ++x) {
      pixels[pixelCount++] = {static_cast<uint8_t>(checkerX * kCheckerSize + x),
                               static_cast<uint8_t>(kFlagStartY + checkerY * kCheckerSize + y), color};
    }
  }
}

void playFinishFlagSweep(Chain& chain, const uint8_t* deviceIds, uint8_t matrixCount,
                         uint8_t* operationStatus, uint32_t durationMs,
                         uint8_t maximumBrightness) {
  if (matrixCount > kSupportedMatrixCount) return;

  const uint8_t checkerGroups = (matrixCount * kMatrixWidth) / kCheckerSize;
  const uint8_t brightnessStepCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);
  const uint32_t totalSteps = checkerGroups * brightnessStepCount;

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBBrightness(deviceIds[matrix], 0, operationStatus);
    chain.setRGBClear(deviceIds[matrix], operationStatus);
  }

  const uint32_t startedAtMs = millis();
  uint32_t step = 0;
  for (uint8_t group = 0; group < checkerGroups; ++group) {
    const uint8_t checkerX = checkerGroups - 1 - group;
    const uint8_t matrix = checkerX / kGroupsPerMatrix;
    const uint8_t localGroup = checkerX % kGroupsPerMatrix;
    RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
    uint8_t pixelCount = 0;
    addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2, kWhite);
    chain.setRGBPixel(deviceIds[matrix], pixels, pixelCount, operationStatus);

    for (uint8_t level : kBrightnessLevels) {
      const uint8_t brightness = (static_cast<uint16_t>(maximumBrightness) * level) / 100;
      chain.setRGBBrightness(deviceIds[matrix], brightness, operationStatus);
      ++step;

      const uint32_t targetMs = startedAtMs + (durationMs * step) / totalSteps;
      const uint32_t nowMs = millis();
      if (nowMs < targetMs) delay(targetMs - nowMs);
    }
    chain.setRGBClear(deviceIds[matrix], operationStatus);
  }

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
    chain.setRGBBrightness(deviceIds[matrix], maximumBrightness, operationStatus);
  }
}

}  // namespace StartupAnimation
