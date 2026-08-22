#include <M5Chain.h>
#include <M5Unified.h>

namespace {

constexpr gpio_num_t kChainRxPin = GPIO_NUM_1;
constexpr gpio_num_t kChainTxPin = GPIO_NUM_2;
constexpr uint32_t kChainBaudRate = 115200;
constexpr uint8_t kBrightnessPercent = 25;

constexpr uint8_t kMatrixCount = 3;
constexpr uint8_t kSectionSize = 4;
constexpr uint8_t kSectionStart = 2;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kGreenCorner = 0x01E0;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kYellowCorner = 0x39E0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kRedCorner = 0x3800;

constexpr uint16_t kMinTestRpm = 3000;
constexpr uint16_t kRedlineRpm = 6200;
constexpr uint16_t kMaxTestRpm = 7000;
constexpr uint16_t kRpmStep = 100;
constexpr uint32_t kRpmStepMs = 75;
constexpr uint8_t kPeakHoldSteps = 20;
constexpr uint32_t kRedFlashHalfPeriodMs = 150;

Chain chain;
uint8_t rgbDeviceIds[kMatrixCount] = {};
uint8_t operationStatus = 0;
uint16_t rpm = kMinTestRpm;
int8_t rpmDirection = 1;
uint8_t peakHoldSteps = 0;
uint8_t lastRenderedStage = 0xFF;
bool redlineModeActive = false;
bool redlineFlashOn = false;
uint32_t lastRedFlashMs = 0;
uint32_t lastRpmStepMs = 0;

void showFatal(const char* detail) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("CHAIN RGB ERROR", M5.Display.width() / 2, 42);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(detail, M5.Display.width() / 2, 72);
}

bool findAndInitialiseMatrices() {
  uint16_t deviceCount = 0;
  if (chain.getDeviceNum(&deviceCount) != CHAIN_OK || deviceCount == 0) {
    return false;
  }

  auto* devices = static_cast<device_info_t*>(malloc(sizeof(device_info_t) * deviceCount));
  if (devices == nullptr) {
    return false;
  }

  device_list_t deviceList;
  deviceList.count = deviceCount;
  deviceList.devices = devices;
  uint8_t rgbFound = 0;
  if (chain.getDeviceList(&deviceList)) {
    for (uint16_t i = 0; i < deviceList.count; ++i) {
      const auto& device = deviceList.devices[i];
      Serial.printf("Chain device: id=%u, type=0x%04X\n", device.id, device.device_type);
      if (device.device_type == CHAIN_RGB_TYPE_CODE && rgbFound < kMatrixCount) {
        rgbDeviceIds[rgbFound++] = device.id;
      }
    }
  }
  free(devices);

  if (rgbFound < kMatrixCount) {
    Serial.printf("Need %u Chain RGB matrices; found %u.\n", kMatrixCount, rgbFound);
    return false;
  }

  for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
    const uint8_t id = rgbDeviceIds[matrix];
    if (chain.setRGBMode(id, RGB_PIXEL_MODE, &operationStatus) != CHAIN_OK ||
        operationStatus != 1) {
      return false;
    }
    chain.setRGBRotation(id, RGB_ROTATION_0, &operationStatus);
    chain.setRGBBrightness(id, kBrightnessPercent, &operationStatus);
    chain.setRGBClear(id, &operationStatus);
  }
  return true;
}

uint8_t stageForRpm(uint16_t currentRpm) {
  if (currentRpm >= 6200) return 5;
  if (currentRpm >= 5600) return 4;
  if (currentRpm >= 5000) return 3;
  if (currentRpm >= 4400) return 2;
  if (currentRpm >= 3800) return 1;
  return 0;
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

uint16_t cornerColorFor(uint16_t color) {
  if (color == kGreen) return kGreenCorner;
  if (color == kYellow) return kYellowCorner;
  return kRedCorner;
}

void fillCentralSection(uint16_t (&frame)[64], uint16_t color) {
  const uint16_t cornerColor = cornerColorFor(color);
  for (uint8_t y = kSectionStart; y < kSectionStart + kSectionSize; ++y) {
    for (uint8_t x = kSectionStart; x < kSectionStart + kSectionSize; ++x) {
      const bool isCorner = (x == kSectionStart || x == kSectionStart + kSectionSize - 1) &&
                            (y == kSectionStart || y == kSectionStart + kSectionSize - 1);
      frame[y * 8 + x] = isCorner ? cornerColor : color;
    }
  }
}

void setCentralSections(uint16_t color) {
  RGBPixelInfo pixels[kSectionSize * kSectionSize];
  const uint16_t cornerColor = cornerColorFor(color);
  uint8_t pixelIndex = 0;

  for (uint8_t y = kSectionStart; y < kSectionStart + kSectionSize; ++y) {
    for (uint8_t x = kSectionStart; x < kSectionStart + kSectionSize; ++x) {
      const bool isCorner = (x == kSectionStart || x == kSectionStart + kSectionSize - 1) &&
                            (y == kSectionStart || y == kSectionStart + kSectionSize - 1);
      pixels[pixelIndex++] = {x, y, isCorner ? cornerColor : color};
    }
  }

  for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
    chain.setRGBPixel(rgbDeviceIds[matrix], pixels, pixelIndex, &operationStatus);
  }
}

void renderRedline(bool flashOn) {
  if (!redlineModeActive) {
    uint16_t redFrames[kMatrixCount][64];
    for (auto& frame : redFrames) {
      for (auto& pixel : frame) {
        pixel = kRed;
      }
    }
    sendFrames(redFrames);
    redlineModeActive = true;
    redlineFlashOn = true;
  }

  if (flashOn != redlineFlashOn) {
    setAllBrightness(flashOn ? kBrightnessPercent : 0);
    redlineFlashOn = flashOn;
  }
}

void renderShiftlight(uint16_t currentRpm, uint8_t stage) {
  if (stage == 5) {
    renderRedline((millis() / kRedFlashHalfPeriodMs) % 2 == 0);
    return;
  }

  const bool wasRedlineMode = redlineModeActive;
  if (wasRedlineMode) {
    setAllBrightness(kBrightnessPercent);
    redlineModeActive = false;
  }

  uint16_t frames[kMatrixCount][64] = {};
  if (stage == 1) {
    fillCentralSection(frames[2], kGreen);
  } else if (stage == 2) {
    fillCentralSection(frames[2], kGreen);
    fillCentralSection(frames[1], kGreen);
  } else if (stage == 3) {
    fillCentralSection(frames[2], kGreen);
    fillCentralSection(frames[1], kGreen);
    fillCentralSection(frames[0], kYellow);
  } else if (stage == 4) {
    if (wasRedlineMode) {
      for (auto& frame : frames) {
        fillCentralSection(frame, kRed);
      }
      sendFrames(frames);
      return;
    }
    setCentralSections(kRed);
    return;
  }
  sendFrames(frames);
}

void renderScreen(uint16_t currentRpm, uint8_t stage) {
  char rpmText[8];
  char stageText[20];
  snprintf(rpmText, sizeof(rpmText), "%u", currentRpm);
  snprintf(stageText, sizeof(stageText), "POINT %u/5", stage);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  const uint8_t rpmTextSize = M5.Display.width() / M5.Display.textWidth(rpmText);
  M5.Display.setTextSize(rpmTextSize);
  M5.Display.drawString(rpmText, M5.Display.width() / 2, 48);
  M5.Display.setTextColor(stage >= 4 ? kRed : kGreen, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("RPM", M5.Display.width() / 2, 82);
  M5.Display.drawString(stage == 5 ? "REDLINE FLASH" : stageText, M5.Display.width() / 2, 106);
}

void updateVisualisation() {
  const uint8_t stage = stageForRpm(rpm);
  if (stage != lastRenderedStage || stage == 5) {
    renderShiftlight(rpm, stage);
    lastRenderedStage = stage;
  }
  renderScreen(rpm, stage);
  Serial.printf("RPM=%u, point=%u/5\n", rpm, stage);
}

void advanceRpm() {
  if (rpm == kMaxTestRpm) {
    if (peakHoldSteps++ < kPeakHoldSteps) {
      return;
    }
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
  M5.Display.drawString("TEST 02: 3 RGB", M5.Display.width() / 2, 64);

  Serial.println("Shiftlight test 02: three central blocks");
  chain.begin(&Serial2, kChainBaudRate, kChainRxPin, kChainTxPin);
  if (!findAndInitialiseMatrices()) {
    showFatal("Check Chain RGB OUT -> IN");
    while (true) {
      delay(250);
    }
  }

  updateVisualisation();
  lastRedFlashMs = millis();
  lastRpmStepMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (rpm >= kRedlineRpm && now - lastRedFlashMs >= kRedFlashHalfPeriodMs) {
    updateVisualisation();
    lastRedFlashMs = now;
  }
  if (now - lastRpmStepMs < kRpmStepMs) {
    return;
  }
  advanceRpm();
  updateVisualisation();
  lastRpmStepMs = now;
}
