#include <cassert>

#include <engine_start_detector.h>

int main() {
  using Shiftlight::EngineStartDetector;

  // При включении контроллера на уже работающем двигателе тест не запускается.
  EngineStartDetector alreadyRunning;
  assert(!alreadyRunning.update(true, 750, 0));
  assert(!alreadyRunning.update(true, 750, 200));

  // Подтверждённая остановка и новый запуск дают ровно одно событие.
  assert(!alreadyRunning.update(true, 0, 300));
  assert(!alreadyRunning.update(true, 0, 1300));
  assert(!alreadyRunning.update(true, 700, 1400));
  assert(alreadyRunning.update(true, 700, 1550));
  assert(!alreadyRunning.update(true, 700, 1700));

  // Наблюдаемая прокрутка позволяет распознать первый запуск без кадра RPM=0.
  EngineStartDetector cranking;
  assert(!cranking.update(true, 250, 0));
  assert(!cranking.update(true, 250, 100));
  assert(!cranking.update(true, 700, 120));
  assert(cranking.update(true, 700, 270));

  // Короткий пропуск RPM не создаёт новый запуск; длительный сбрасывает состояние.
  EngineStartDetector dropout;
  assert(!dropout.update(true, 750, 0));
  assert(!dropout.update(true, 750, 150));
  assert(!dropout.update(false, 0, 200));
  assert(!dropout.update(true, 250, 1200));
  assert(!dropout.update(true, 250, 1400));
  assert(!dropout.update(true, 750, 1500));
  assert(!dropout.update(true, 750, 1700));
  assert(!dropout.update(false, 0, 1800));
  assert(!dropout.update(false, 0, 3800));
  assert(!dropout.update(true, 250, 3900));
  assert(!dropout.update(true, 250, 4000));
  assert(!dropout.update(true, 750, 4100));
  assert(dropout.update(true, 750, 4250));

  EngineStartDetector lateReconnect;
  assert(!lateReconnect.update(true, 750, 0));
  assert(!lateReconnect.update(true, 750, 150));
  assert(!lateReconnect.update(false, 0, 200));
  assert(!lateReconnect.update(false, 0, 2200));
  assert(!lateReconnect.update(true, 750, 2300));
  assert(!lateReconnect.update(true, 750, 2500));

  assert(ShiftlightConfig::litLedCount(4299) == 0);
  assert(ShiftlightConfig::litLedCount(4300) == 1);
  assert(ShiftlightConfig::litLedCount(6100) == 10);
}
