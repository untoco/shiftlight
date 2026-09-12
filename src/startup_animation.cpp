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
constexpr uint8_t kFadeLevels[] = {100, 80, 60, 40, 20, 0};

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
  const uint8_t fadeStepCount = sizeof(kFadeLevels) / sizeof(kFadeLevels[0]);

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

  for (uint8_t step = 0; step < fadeStepCount; ++step) {
    const uint8_t brightness = (static_cast<uint16_t>(maximumBrightness) * kFadeLevels[step]) / 100;
    for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
      chain.setRGBBrightness(deviceIds[matrix], brightness, operationStatus);
    }

    const uint32_t targetMs = startedAtMs + fillDurationMs +
                              (fadeDurationMs * (step + 1)) / fadeStepCount;
    const uint32_t nowMs = millis();
    if (nowMs < targetMs) delay(targetMs - nowMs);
  }

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
    chain.setRGBBrightness(deviceIds[matrix], maximumBrightness, operationStatus);
  }
}

}  // namespace StartupAnimation
