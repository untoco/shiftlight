#include "startup_animation.h"

#include <M5Chain.h>

namespace StartupAnimation {
namespace {

constexpr uint8_t kMatrixWidth = 8;
constexpr uint8_t kMatrixHeight = 8;
constexpr uint8_t kFlagStartY = 4;
constexpr uint8_t kCheckerSize = 2;
constexpr uint32_t kFrameIntervalMs = 20;
constexpr uint8_t kSupportedMatrixCount = 3;
constexpr uint8_t kGroupsPerMatrix = kMatrixWidth / kCheckerSize;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kBlack = 0x0000;
constexpr uint8_t kDitherPhases = 8;
constexpr uint8_t kDitherOrder[kDitherPhases] = {0, 4, 2, 6, 1, 5, 3, 7};

}  // namespace

uint8_t fadeIntensity(uint32_t elapsedMs, uint32_t fadeDurationMs) {
  const uint32_t halfDurationMs = fadeDurationMs / 2;
  if (elapsedMs >= fadeDurationMs || halfDurationMs == 0) return 0;
  if (elapsedMs <= halfDurationMs) return (elapsedMs * 255) / halfDurationMs;
  return ((fadeDurationMs - elapsedMs) * 255) / halfDurationMs;
}

bool isDitherOn(uint8_t intensity, uint8_t phase) {
  const uint8_t whiteFrames = (static_cast<uint16_t>(intensity) * kDitherPhases + 254) / 255;
  return whiteFrames > kDitherOrder[phase % kDitherPhases];
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
    const uint8_t ditherPhase = (elapsedMs / kFrameIntervalMs) % kDitherPhases;

    for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
      RGBPixelInfo pixels[kGroupsPerMatrix * kCheckerSize * kCheckerSize] = {};
      uint8_t pixelCount = 0;

      for (uint8_t localGroup = 0; localGroup < kGroupsPerMatrix; ++localGroup) {
        const uint8_t checkerX = matrix * kGroupsPerMatrix + localGroup;
        const uint32_t groupStartMs = (checkerGroups - 1 - checkerX) * groupIntervalMs;
        const uint8_t fade = elapsedMs < groupStartMs
                                 ? 0
                                 : fadeIntensity(elapsedMs - groupStartMs, fadeDurationMs);
        const uint8_t intensity = isDitherOn(fade, ditherPhase + checkerX) ? 255 : 0;
        if (intensity == previousIntensity[matrix][localGroup]) continue;

        addCheckerSquare(pixels, pixelCount, localGroup, checkerX % 2,
                         intensity == 0 ? kBlack : kWhite);
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
