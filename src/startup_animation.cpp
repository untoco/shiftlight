#include "startup_animation.h"

#include <M5Chain.h>

namespace StartupAnimation {
namespace {

constexpr uint8_t kMatrixWidth = 8;
constexpr uint8_t kMatrixHeight = 8;
constexpr uint8_t kFlagStartY = 4;
constexpr uint8_t kCheckerSize = 2;
constexpr uint16_t kWhite = 0xFFFF;

}  // namespace

void showFinishFlag(Chain& chain, const uint8_t* deviceIds, uint8_t matrixCount,
                    uint8_t* operationStatus, uint32_t durationMs) {
  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    uint16_t frame[kMatrixWidth * kMatrixHeight] = {};
    for (uint8_t y = kFlagStartY; y < kMatrixHeight; ++y) {
      for (uint8_t x = 0; x < kMatrixWidth; ++x) {
        const uint8_t checkerX = (matrix * kMatrixWidth + x) / kCheckerSize;
        const uint8_t checkerY = (y - kFlagStartY) / kCheckerSize;
        if ((checkerX + checkerY) % 2 == 0) {
          frame[y * kMatrixWidth + x] = kWhite;
        }
      }
    }
    chain.setRGBBufferRefresh(deviceIds[matrix], frame, operationStatus);
  }

  delay(durationMs);

  for (uint8_t matrix = 0; matrix < matrixCount; ++matrix) {
    chain.setRGBClear(deviceIds[matrix], operationStatus);
  }
}

}  // namespace StartupAnimation
