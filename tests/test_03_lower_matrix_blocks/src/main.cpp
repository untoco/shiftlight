#include <M5Chain.h>
#include <M5Unified.h>

namespace {

constexpr gpio_num_t kChainRxPin = GPIO_NUM_1;
constexpr gpio_num_t kChainTxPin = GPIO_NUM_2;
constexpr uint32_t kChainBaudRate = 115200;
constexpr uint8_t kBrightnessPercent = 25;

constexpr uint8_t kMatrixCount = 3;
constexpr uint8_t kSectionSize = 4;
constexpr uint8_t kSectionStartX = 2;
constexpr uint8_t kSectionStartY = 4;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kGreenCorner = 0x01E0;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kYellowCorner = 0x39E0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kRedCorner = 0x3800;

constexpr uint16_t kMinTestRpm = 3000;
constexpr uint16_t kRedlineRpm = 6500;
constexpr uint16_t kMaxTestRpm = 7000;
constexpr uint16_t kRpmStep = 100;
constexpr uint32_t kRpmStepMs = 75;
constexpr uint8_t kPeakHoldSteps = 20;
constexpr uint32_t kRedFlashHalfPeriodMs = 150;

class FastChain : public Chain {
 public:
  void setLowerSectionsPipelined(const uint8_t (&deviceIds)[kMatrixCount],
                                 const uint16_t (&colors)[kMatrixCount]) {
    if (!acquireMutex()) return;

    for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
      cmdBufferSize = 0;
      cmdBuffer[cmdBufferSize++] = kSectionSize * kSectionSize;
      for (uint8_t y = kSectionStartY; y < kSectionStartY + kSectionSize; ++y) {
        for (uint8_t x = kSectionStartX; x < kSectionStartX + kSectionSize; ++x) {
          const bool corner = (x == kSectionStartX || x == kSectionStartX + kSectionSize - 1) &&
                              (y == kSectionStartY || y == kSectionStartY + kSectionSize - 1);
          const uint16_t color = corner ? cornerColorFor(colors[matrix]) : colors[matrix];
          cmdBuffer[cmdBufferSize++] = ((x & 0x07) << 3) | (y & 0x07);
          cmdBuffer[cmdBufferSize++] = color & 0xFF;
          cmdBuffer[cmdBufferSize++] = (color >> 8) & 0xFF;
        }
      }
      sendPacket(deviceIds[matrix], CHAIN_RGB_SET_PIXEL, cmdBuffer, cmdBufferSize);
    }

    delay(20);
    processIncomingData();
    releaseMutex();
  }

 private:
  static uint16_t cornerColorFor(uint16_t color) {
    if (color == kGreen) return kGreenCorner;
    if (color == kYellow) return kYellowCorner;
    return kRedCorner;
  }
};

FastChain chain;
uint8_t rgbDeviceIds[kMatrixCount] = {};
uint8_t operationStatus = 0;
uint16_t rpm = kMinTestRpm;
int8_t rpmDirection = 1;
uint8_t peakHoldSteps = 0;
uint8_t lastRenderedStage = 0xFF;
uint32_t lastRpmStepMs = 0;
bool redlineModeActive = false;
bool redlineFlashOn = false;

void showFatal(const char* detail) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("CHAIN RGB ERROR", M5.Display.width() / 2, 42);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(detail, M5.Display.width() / 2, 72);
}

bool initialiseMatrices() {
  chain.begin(&Serial2, kChainBaudRate, kChainRxPin, kChainTxPin);
  uint16_t deviceCount = 0;
  if (chain.getDeviceNum(&deviceCount) != CHAIN_OK || deviceCount == 0) return false;

  auto* devices = static_cast<device_info_t*>(malloc(sizeof(device_info_t) * deviceCount));
  if (devices == nullptr) return false;

  device_list_t deviceList{deviceCount, devices};
  uint8_t rgbFound = 0;
  if (chain.getDeviceList(&deviceList)) {
    for (uint16_t i = 0; i < deviceList.count; ++i) {
      if (deviceList.devices[i].device_type == CHAIN_RGB_TYPE_CODE && rgbFound < kMatrixCount) {
        rgbDeviceIds[rgbFound++] = deviceList.devices[i].id;
      }
    }
  }
  free(devices);
  if (rgbFound != kMatrixCount) return false;

  for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
    const uint8_t id = rgbDeviceIds[matrix];
    if (chain.setRGBMode(id, RGB_PIXEL_MODE, &operationStatus) != CHAIN_OK || operationStatus != 1) {
      return false;
    }
    chain.setRGBRotation(id, RGB_ROTATION_0, &operationStatus);
    chain.setRGBBrightness(id, kBrightnessPercent, &operationStatus);
    chain.setRGBClear(id, &operationStatus);
  }
  return true;
}

uint8_t stageForRpm(uint16_t currentRpm) {
  if (currentRpm >= kRedlineRpm) return 5;
  if (currentRpm >= 6200) return 4;
  if (currentRpm >= 5500) return 3;
  if (currentRpm >= 4800) return 2;
  if (currentRpm >= 4100) return 1;
  return 0;
}

uint16_t cornerColorFor(uint16_t color) {
  if (color == kGreen) return kGreenCorner;
  if (color == kYellow) return kYellowCorner;
  return kRedCorner;
}

void fillLowerSection(uint16_t (&frame)[64], uint16_t color) {
  for (uint8_t y = kSectionStartY; y < kSectionStartY + kSectionSize; ++y) {
    for (uint8_t x = kSectionStartX; x < kSectionStartX + kSectionSize; ++x) {
      const bool corner = (x == kSectionStartX || x == kSectionStartX + kSectionSize - 1) &&
                          (y == kSectionStartY || y == kSectionStartY + kSectionSize - 1);
      frame[y * 8 + x] = corner ? cornerColorFor(color) : color;
    }
  }
}

void sendFrames(uint16_t (&frames)[kMatrixCount][64]) {
  for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
    chain.setRGBBufferRefresh(rgbDeviceIds[matrix], frames[matrix], &operationStatus);
  }
}

void setAllBrightness(uint8_t brightness) {
  for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
    chain.setRGBBrightness(rgbDeviceIds[matrix], brightness, &operationStatus);
  }
}

void renderStage(uint8_t stage) {
  if (stage == 5) {
    if (!redlineModeActive) {
      uint16_t frames[kMatrixCount][64] = {};
      for (auto& frame : frames) {
        for (uint8_t y = kSectionStartY; y < 8; ++y) {
          for (uint8_t x = 0; x < 8; ++x) {
            frame[y * 8 + x] = kRed;
          }
        }
      }
      sendFrames(frames);
      redlineModeActive = true;
      redlineFlashOn = true;
    }
    const bool flashOn = (millis() / kRedFlashHalfPeriodMs) % 2 == 0;
    if (flashOn != redlineFlashOn) {
      setAllBrightness(flashOn ? kBrightnessPercent : 0);
      redlineFlashOn = flashOn;
    }
    return;
  }

  const bool wasRedlineMode = redlineModeActive;
  if (wasRedlineMode) {
    setAllBrightness(kBrightnessPercent);
    redlineModeActive = false;
  }

  uint16_t frames[kMatrixCount][64] = {};
  if (stage == 1) {
    fillLowerSection(frames[2], kGreen);
  } else if (stage == 2) {
    fillLowerSection(frames[2], kGreen);
    fillLowerSection(frames[1], kGreen);
  } else if (stage == 3 || stage == 4) {
    const uint16_t colors[kMatrixCount] = {
        stage == 3 ? kYellow : kRed,
        stage == 3 ? kGreen : kRed,
        stage == 3 ? kGreen : kRed,
    };
    if (!wasRedlineMode) {
      chain.setLowerSectionsPipelined(rgbDeviceIds, colors);
      return;
    }
    for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
      fillLowerSection(frames[matrix], colors[matrix]);
    }
  }
  sendFrames(frames);
}

void renderScreen(uint8_t stage) {
  char rpmText[8] = {};
  snprintf(rpmText, sizeof(rpmText), "%u", rpm);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextSize(M5.Display.width() / M5.Display.textWidth(rpmText));
  M5.Display.drawString(rpmText, M5.Display.width() / 2, 45);
  M5.Display.setTextColor(stage >= 4 ? kRed : stage == 3 ? kYellow : kGreen, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("RPM", M5.Display.width() / 2, 79);
  char pointText[20] = {};
  snprintf(pointText, sizeof(pointText), stage == 5 ? "REDLINE FLASH" : "POINT %u/5", stage);
  M5.Display.drawString(pointText, M5.Display.width() / 2, 104);
}

void updateVisualisation() {
  const uint8_t stage = stageForRpm(rpm);
  if (stage != lastRenderedStage || stage == 5) {
    renderStage(stage);
    lastRenderedStage = stage;
  }
  renderScreen(stage);
}

void advanceRpm() {
  if (rpm == kMaxTestRpm) {
    if (peakHoldSteps++ < kPeakHoldSteps) return;
    peakHoldSteps = 0;
    rpmDirection = -1;
  } else if (rpm == kMinTestRpm) {
    rpmDirection = 1;
  }
  rpm += rpmDirection * kRpmStep;
}

}  // namespace

void setup() {
  auto config = M5.config();
  M5.begin(config);
  Serial.begin(115200);
  delay(200);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawString("TEST 03: LOWER", M5.Display.width() / 2, 64);

  chain.begin(&Serial2, kChainBaudRate, kChainRxPin, kChainTxPin);
  if (!initialiseMatrices()) {
    showFatal("Check 3 Chain RGB");
    while (true) delay(250);
  }
  updateVisualisation();
  lastRpmStepMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastRpmStepMs < kRpmStepMs) return;
  advanceRpm();
  updateVisualisation();
  lastRpmStepMs = now;
}
