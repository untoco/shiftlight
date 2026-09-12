#pragma once

#include <stdint.h>

namespace ShiftlightConfig {

struct ShiftPoints {
  uint16_t first;
  uint16_t second;
  uint16_t third;
  uint16_t allRed;
  uint16_t redline;
};

// Рабочая карта переключений. Калибруется здесь, без изменения логики индикации.
constexpr ShiftPoints kShiftPoints = {4500, 5100, 5700, 6300, 6500};

inline uint8_t stageForRpm(uint16_t rpm) {
  if (rpm >= kShiftPoints.redline) return 5;
  if (rpm >= kShiftPoints.allRed) return 4;
  if (rpm >= kShiftPoints.third) return 3;
  if (rpm >= kShiftPoints.second) return 2;
  if (rpm >= kShiftPoints.first) return 1;
  return 0;
}

struct DemoRpmSettings {
  uint16_t minimum;
  uint16_t maximum;
  uint16_t step;
  uint32_t stepIntervalMs;
  uint8_t peakHoldSteps;
};

// Единый цикл оборотов для всех визуализаций.
constexpr DemoRpmSettings kDemoRpm = {3000, 7000, 100, 75, 20};

enum class VisualizationType : uint8_t {
  kCentralBlocks,
  kLowerBlocks,
};

// Выбор геометрии основной прошивки.
constexpr VisualizationType kVisualizationType = VisualizationType::kLowerBlocks;

struct VisualizationSettings {
  uint8_t sectionStartX;
  uint8_t sectionStartY;
  uint8_t redlineStartY;
};

constexpr VisualizationSettings kVisualization =
    kVisualizationType == VisualizationType::kLowerBlocks
        ? VisualizationSettings{2, 4, 4}
        : VisualizationSettings{2, 2, 0};

}  // namespace ShiftlightConfig
