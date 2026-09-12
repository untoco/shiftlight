#include "startup_animation.h"

#include <M5Chain.h>

namespace StartupAnimation {
namespace {

constexpr uint8_t kMatrixWidth = 8;
constexpr uint8_t kMatrixHeight = 8;
constexpr uint8_t kFlagStartY = 4;
constexpr uint8_t kCheckerSize = 2;
constexpr uint32_t kFrameIntervalMs = 40;

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

void playFinishFlagSweep(Chain& chain, const uint8_t* deviceIds, uint8_t matrixCount,
                         uint8_t* operationStatus, uint32_t durationMs) {
  const uint8_t checkerGroups = (matrixCount * kMatrixWidth) / kCheckerSize;
  const uint32_t groupIntervalMs = durationMs / (checkerGroups + 3);
  const uint32_t fadeDurationMs = groupIntervalMs * 4;

  for (uint32_t elapsedMs = 0; elapsedMs < durationMs; elapsedMs += kFrameIntervalMs) {
    uint16_t frames[3][kMatrixWidth * kMatrixHeight] = {};

    for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
      for (uint8_t y = kFlagStartY; y < kMatrixHeight; ++y) {
        for (uint8_t x = 0; x < kMatrixWidth; ++x) {
          const uint8_t checkerX = (matrix * kMatrixWidth + x) / kCheckerSize;
          const uint8_t checkerY = (y - kFlagStartY) / kCheckerSize;
          if ((checkerX + checkerY) % 2 != 0) continue;

          const uint32_t groupStartMs = (checkerGroups - 1 - checkerX) * groupIntervalMs;
          if (elapsedMs < groupStartMs) continue;
          frames[matrix][y * kMatrixWidth + x] =
              grayscale(fadeIntensity(elapsedMs - groupStartMs, fadeDurationMs));
        }
      }
      chain.setRGBBufferRefresh(deviceIds[matrix], frames[matrix], operationStatus);
    }
    delay(kFrameIntervalMs);
  }

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
  }
}

}  // namespace StartupAnimation
