#pragma once

#include <shiftlight_config.h>

namespace Shiftlight {

class EngineStartDetector {
 public:
  // Возвращает true только при подтверждённом переходе остановка/прокрутка → работа.
  bool update(bool rpmFresh, uint16_t rpm, uint32_t nowMs) {
    if (!rpmFresh) {
      lowCandidate_ = false;
      crankCandidate_ = false;
      highCandidate_ = false;
      if (!missingCandidate_) {
        missingCandidate_ = true;
        missingSinceMs_ = nowMs;
      }
      if (state_ == State::kRunning &&
          nowMs - missingSinceMs_ >= ShiftlightConfig::kRpmMissingResetMs) {
        state_ = State::kUnknown;
      }
      return false;
    }

    missingCandidate_ = false;
    if (rpm <= ShiftlightConfig::kStoppedRpm) {
      crankCandidate_ = false;
      highCandidate_ = false;
      if (!lowCandidate_) {
        lowCandidate_ = true;
        lowSinceMs_ = nowMs;
      }
      if (nowMs - lowSinceMs_ >= ShiftlightConfig::kStoppedConfirmMs) {
        state_ = State::kStopped;
      }
      return false;
    }

    lowCandidate_ = false;
    if (rpm < ShiftlightConfig::kRunningRpm) {
      highCandidate_ = false;
      if (!crankCandidate_) {
        crankCandidate_ = true;
        crankSinceMs_ = nowMs;
      }
      if (state_ == State::kUnknown &&
          nowMs - crankSinceMs_ >= ShiftlightConfig::kCrankingConfirmMs) {
        state_ = State::kStopped;
      }
      return false;
    }

    crankCandidate_ = false;
    if (!highCandidate_) {
      highCandidate_ = true;
      highSinceMs_ = nowMs;
    }
    if (nowMs - highSinceMs_ < ShiftlightConfig::kRunningConfirmMs) return false;

    const bool started = state_ == State::kStopped;
    state_ = State::kRunning;
    return started;
  }

 private:
  enum class State : uint8_t { kUnknown, kStopped, kRunning };
  State state_ = State::kUnknown;
  bool lowCandidate_ = false;
  bool crankCandidate_ = false;
  bool highCandidate_ = false;
  bool missingCandidate_ = false;
  uint32_t lowSinceMs_ = 0;
  uint32_t crankSinceMs_ = 0;
  uint32_t highSinceMs_ = 0;
  uint32_t missingSinceMs_ = 0;
};

}  // namespace Shiftlight
