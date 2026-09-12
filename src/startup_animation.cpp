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
constexpr uint16_t kLightGray = 0xBDF7;
constexpr uint16_t kMidGray = 0x6B4D;
// Слегка холодный тёмно-серый: меньшая доля красного компенсирует тёплый след LED.
constexpr uint16_t kDarkCoolGray = 0x21CA;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kFadeColors[] = {kLightGray, kMidGray, kDarkCoolGray, kBlack};

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
  const uint32_t fillDurationMs = durationMs / 2;
  const uint32_t fadeDurationMs = durationMs - fillDurationMs;
  const uint8_t fadeStepCount = sizeof(kFadeColors) / sizeof(kFadeColors[0]);

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
    chain.setRGBBrightness(deviceIds[matrix], maximumBrightness, operationStatus);
  }

  const uint32_t startedAtMs = millis();
  for (uint8_t group = 0; group < checkerGroups; ++group) {
    const uint8_t checkerX = checkerGroups - 1 - group;
    const uint8_t matrix = checkerX / kGroupsPerMatrix;
    const uint8_t localGroup = checkerX % kGroupsPerMatrix;
    RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
    uint8_t pixelCount = 0;
    addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2, kWhite);
    chain.setRGBPixel(deviceIds[matrix], pixels, pixelCount, operationStatus);

    const uint32_t targetMs = startedAtMs + (fillDurationMs * (group + 1)) / checkerGroups;
    const uint32_t nowMs = millis();
    if (nowMs < targetMs) delay(targetMs - nowMs);
  }

  uint32_t fadeStep = 0;
  for (uint8_t group = 0; group < checkerGroups; ++group) {
    const uint8_t checkerX = checkerGroups - 1 - group;
    const uint8_t matrix = checkerX / kGroupsPerMatrix;
    const uint8_t localGroup = checkerX % kGroupsPerMatrix;

    for (uint16_t color : kFadeColors) {
      RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
      uint8_t pixelCount = 0;
      addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2, color);
      chain.setRGBPixel(deviceIds[matrix], pixels, pixelCount, operationStatus);
      ++fadeStep;

      const uint32_t targetMs = startedAtMs + fillDurationMs +
                                (fadeDurationMs * fadeStep) / (checkerGroups * fadeStepCount);
      const uint32_t nowMs = millis();
      if (nowMs < targetMs) delay(targetMs - nowMs);
    }
  }

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
    chain.setRGBBrightness(deviceIds[matrix], maximumBrightness, operationStatus);
  }
}

}  // namespace StartupAnimation
