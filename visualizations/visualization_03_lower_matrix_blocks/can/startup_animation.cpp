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
constexpr uint32_t kFadeStepIntervalMs = 20;

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
  const uint8_t fadeStepCount = sizeof(kFadeColors) / sizeof(kFadeColors[0]);
  const uint32_t fadeDurationMs = kFadeStepIntervalMs * fadeStepCount;
  const uint32_t initialHoldMs = durationMs / 4;
  const uint32_t waveDurationMs = durationMs - initialHoldMs - fadeDurationMs;
  const uint32_t groupIntervalMs =
      checkerGroups > 1 ? waveDurationMs / (checkerGroups - 1) : 0;
  bool appeared[kSupportedMatrixCount * kGroupsPerMatrix] = {};
  uint8_t nextFadeStep[kSupportedMatrixCount * kGroupsPerMatrix] = {};

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
    chain.setRGBBrightness(deviceIds[matrix], maximumBrightness, operationStatus);
  }

  const uint32_t startedAtMs = millis();
  while (millis() - startedAtMs < durationMs) {
    const uint32_t elapsedMs = millis() - startedAtMs;

    for (uint8_t group = 0; group < checkerGroups; ++group) {
      const uint8_t checkerX = checkerGroups - 1 - group;
      const uint8_t matrix = checkerX / kGroupsPerMatrix;
      const uint8_t localGroup = checkerX % kGroupsPerMatrix;
      const uint32_t appearAtMs = group * groupIntervalMs;

      if (!appeared[group] && elapsedMs >= appearAtMs) {
        RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
        uint8_t pixelCount = 0;
        addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2, kWhite);
        chain.setRGBPixel(deviceIds[matrix], pixels, pixelCount, operationStatus);
        appeared[group] = true;
      }

      const uint32_t fadeAtMs = appearAtMs + initialHoldMs;
      while (appeared[group] && nextFadeStep[group] < fadeStepCount &&
             elapsedMs >= fadeAtMs + nextFadeStep[group] * kFadeStepIntervalMs) {
        const uint16_t color = kFadeColors[nextFadeStep[group]++];
      RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
      uint8_t pixelCount = 0;
      addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2, color);
      chain.setRGBPixel(deviceIds[matrix], pixels, pixelCount, operationStatus);
      }
    }
    delay(5);
  }

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
    chain.setRGBBrightness(deviceIds[matrix], maximumBrightness, operationStatus);
  }
}

}  // namespace StartupAnimation
