#include <M5Chain.h>
#include <M5Unified.h>

namespace {

constexpr gpio_num_t kChainRxPin = GPIO_NUM_1;
constexpr gpio_num_t kChainTxPin = GPIO_NUM_2;
constexpr uint32_t kChainBaudRate = 115200;
constexpr uint8_t kBrightnessPercent = 25;

constexpr uint8_t kMatrixCount = 3;
constexpr uint8_t kSectionCount = 5;
constexpr uint8_t kSectionWidth = 3;
constexpr uint8_t kSectionGap = 2;
constexpr uint8_t kCentralRow = 2;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kRed = 0xF800;

constexpr uint16_t kMinTestRpm = 3000;
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
    Serial.println("No Chain devices found.");
    return false;
  }

  auto* devices = static_cast<device_info_t*>(malloc(sizeof(device_info_t) * deviceCount));
  if (devices == nullptr) {
    Serial.println("Out of memory while reading Chain devices.");
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
      Serial.printf("Could not enable pixel mode on matrix %u.\n", matrix + 1);
      return false;
    }
    chain.setRGBRotation(id, RGB_ROTATION_0, &operationStatus);
    chain.setRGBBrightness(id, kBrightnessPercent, &operationStatus);
    chain.setRGBClear(id, &operationStatus);
  }

  Serial.printf("Shiftlight test ready: %u matrices, %u sections.\n", kMatrixCount,
                kSectionCount);
  return true;
}

uint8_t activeSectionsForRpm(uint16_t currentRpm) {
  if (currentRpm < 4000) {
    return 0;
  }
  const uint8_t sections = ((currentRpm - 4000) / 500) + 1;
  return sections > kSectionCount ? kSectionCount : sections;
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

void renderShiftlight(uint16_t currentRpm, uint8_t activeSections) {
  if (currentRpm >= kMaxTestRpm) {
    renderRedline((millis() / kRedFlashHalfPeriodMs) % 2 == 0);
    return;
  }

  if (redlineModeActive) {
    setAllBrightness(kBrightnessPercent);
    redlineModeActive = false;
  }

  uint16_t frames[kMatrixCount][64] = {};

  for (uint8_t section = 0; section < activeSections; ++section) {
    const uint8_t globalX = section * (kSectionWidth + kSectionGap);
    const uint16_t color = currentRpm >= 6500 ? kRed : (section < 3 ? kGreen : kYellow);

    for (uint8_t y = kCentralRow; y < kCentralRow + kSectionWidth; ++y) {
      for (uint8_t x = globalX; x < globalX + kSectionWidth; ++x) {
        const uint8_t matrix = x / 8;
        const uint8_t localX = x % 8;
        frames[matrix][y * 8 + localX] = color;
      }
    }
  }

  sendFrames(frames);
}

void renderScreen(uint16_t currentRpm, uint8_t activeSections) {
  char rpmText[8];
  char sectionsText[20];
  snprintf(rpmText, sizeof(rpmText), "%u", currentRpm);
  snprintf(sectionsText, sizeof(sectionsText), "SECTIONS %u/%u", activeSections,
           kSectionCount);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(3);
  M5.Display.drawString(rpmText, M5.Display.width() / 2, 48);
  const bool redline = currentRpm >= kMaxTestRpm;
  const bool allRed = currentRpm >= 6500;
  M5.Display.setTextColor(redline || allRed ? kRed : kGreen, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("RPM", M5.Display.width() / 2, 82);
  M5.Display.drawString(redline ? "REDLINE FLASH" : (allRed ? "ALL RED" : sectionsText),
                        M5.Display.width() / 2, 106);
}

void updateVisualisation() {
  const uint8_t activeSections = activeSectionsForRpm(rpm);
  renderShiftlight(rpm, activeSections);
  renderScreen(rpm, activeSections);
  Serial.printf("RPM=%u, sections=%u/%u\n", rpm, activeSections, kSectionCount);
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
  M5.Display.drawString("SEARCHING 3 RGB", M5.Display.width() / 2, 64);

  Serial.println("Shiftlight three-matrix visualisation test");
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
  if (rpm >= kMaxTestRpm && now - lastRedFlashMs >= kRedFlashHalfPeriodMs) {
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
