#pragma once

#include <stdint.h>

namespace VisualizationDemo {

struct RpmSettings {
  uint16_t minimum;
  uint16_t maximum;
  uint16_t step;
  uint32_t stepIntervalMs;
  uint8_t peakHoldSteps;
};

// Единый искусственный цикл оборотов только для визуализаций.
constexpr RpmSettings kRpm = {3000, 7000, 100, 75, 20};

}  // namespace VisualizationDemo
