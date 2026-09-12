#pragma once

#include <stdint.h>

class Chain;

namespace StartupAnimation {

void playFinishFlagSweep(Chain& chain, const uint8_t* deviceIds, uint8_t matrixCount,
                         uint8_t* operationStatus, uint32_t durationMs);

}  // namespace StartupAnimation
