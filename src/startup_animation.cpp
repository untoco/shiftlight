#include "startup_animation.h"

#include <M5Chain.h>

namespace StartupAnimation {
namespace {

constexpr uint8_t kMatrixWidth = 8;
constexpr uint8_t kMatrixHeight = 8;
constexpr uint8_t kFlagStartY = 4;
constexpr uint8_t kCheckerSize = 2;
constexpr uint32_t kFrameIntervalMs = 40;
constexpr uint8_t kSupportedMatrixCount = 3;
constexpr uint8_t kGroupsPerMatrix = kMatrixWidth / kCheckerSize;

}  // namespace

uint16_t grayscale(uint8_t intensity) {
  return ((intensity >> 3) << 11) | ((intensity >> 2) << 5) | (intensity >> 3);
}

uint8_t fadeIntensity(uint32_t elapsedMs, uint32_t fadeDurationMs) {
  const uint32_t halfDurationMs = fadeDurationMs / 2;
  if (elapsedMs >= fadeDurationMs || halfDurationMs == 0) return 0;
  if (elapsedMs <= halfDurationMs) return (elapsedMs * 255) / halfDurationMs;
  return ((fadeDurationMs - elapsedMs) * 255) / halfDurationMs;
}

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
                         uint8_t* operationStatus, uint32_t durationMs) {
  if (matrixCount > kSupportedMatrixCount) return;

  const uint8_t checkerGroups = (matrixCount * kMatrixWidth) / kCheckerSize;
  const uint32_t groupIntervalMs = durationMs / (checkerGroups + 3);
  const uint32_t fadeDurationMs = groupIntervalMs * 4;
  uint8_t previousIntensity[kSupportedMatrixCount][kGroupsPerMatrix] = {};

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
  }

  const uint32_t startedAtMs = millis();
  while (millis() - startedAtMs < durationMs) {
    const uint32_t elapsedMs = millis() - startedAtMs;

    for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
      RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
      uint8_t pixelCount = 0;

      for (uint8_t localGroup = 0; localGroup < kGroupsPerMatrix; ++localGroup) {
        const uint8_t checkerX = matrix * kGroupsPerMatrix + localGroup;
        const uint32_t groupStartMs = (checkerGroups - 1 - checkerX) * groupIntervalMs;
        const uint8_t intensity = elapsedMs < groupStartMs
                                      ? 0
                                      : fadeIntensity(elapsedMs - groupStartMs, fadeDurationMs);
        if (intensity == previousIntensity[matrix][localGroup]) continue;

        addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2, grayscale(intensity));
        previousIntensity[matrix][localGroup] = intensity;
      }
      if (pixelCount > 0) {
        chain.setRGBPixel(deviceIds[matrix], pixels, pixelCount, operationStatus);
      }
    }
    delay(kFrameIntervalMs);
  }

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
  }
}

}  // namespace StartupAnimation
