#include <M5Chain.h>
#include <M5Unified.h>
#include "driver/twai.h"

namespace {

constexpr gpio_num_t kCanTxPin = GPIO_NUM_5;
constexpr gpio_num_t kCanRxPin = GPIO_NUM_6;
constexpr gpio_num_t kChainRxPin = GPIO_NUM_1;
constexpr gpio_num_t kChainTxPin = GPIO_NUM_2;
constexpr uint32_t kCanBitRate = 500000;
constexpr uint32_t kChainBaudRate = 115200;

constexpr uint32_t kRpmCanId = 0x0A5;
constexpr uint8_t kRpmLowByte = 5;
constexpr uint8_t kRpmHighByte = 6;
constexpr uint32_t kRpmTimeoutMs = 200;
constexpr uint32_t kScreenRefreshMs = 100;
constexpr uint32_t kRedFlashHalfPeriodMs = 150;
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

class FastChain : public Chain {
 public:
  void setCentralSectionsPipelined(const uint8_t (&deviceIds)[kMatrixCount],
                                   const uint16_t (&colors)[kMatrixCount]) {
    if (!acquireMutex()) {
      return;
    }

    for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
      cmdBufferSize = 0;
      cmdBuffer[cmdBufferSize++] = kSectionSize * kSectionSize;
      for (uint8_t y = kSectionStart; y < kSectionStart + kSectionSize; ++y) {
        for (uint8_t x = kSectionStart; x < kSectionStart + kSectionSize; ++x) {
          const bool corner = (x == kSectionStart || x == kSectionStart + kSectionSize - 1) &&
                              (y == kSectionStart || y == kSectionStart + kSectionSize - 1);
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
uint16_t currentRpm = 0;
uint8_t lastRenderedStage = 0xFF;
uint32_t lastRpmFrameMs = 0;
uint32_t lastScreenRefreshMs = 0;
bool redlineModeActive = false;
bool redlineFlashOn = false;

void showFatal(const char* title, const char* detail) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString(title, M5.Display.width() / 2, 46);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(detail, M5.Display.width() / 2, 76);
}

bool initialiseCan() {
  twai_general_config_t general =
      TWAI_GENERAL_CONFIG_DEFAULT(kCanTxPin, kCanRxPin, TWAI_MODE_LISTEN_ONLY);
  twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  const esp_err_t installResult = twai_driver_install(&general, &timing, &filter);
  if (installResult != ESP_OK) {
    Serial.printf("TWAI install failed: %s\n", esp_err_to_name(installResult));
    return false;
  }

  const esp_err_t startResult = twai_start();
  if (startResult != ESP_OK) {
    Serial.printf("TWAI start failed: %s\n", esp_err_to_name(startResult));
    twai_driver_uninstall();
    return false;
  }

  Serial.printf("PT-CAN ready: %lu bit/s, listen-only, RPM frame 0x%03lX\n",
                static_cast<unsigned long>(kCanBitRate), static_cast<unsigned long>(kRpmCanId));
  return true;
}

bool initialiseMatrices() {
  chain.begin(&Serial2, kChainBaudRate, kChainRxPin, kChainTxPin);
  uint16_t deviceCount = 0;
  if (chain.getDeviceNum(&deviceCount) != CHAIN_OK || deviceCount == 0) {
    return false;
  }

  auto* devices = static_cast<device_info_t*>(malloc(sizeof(device_info_t) * deviceCount));
  if (devices == nullptr) {
    return false;
  }

  device_list_t deviceList{deviceCount, devices};
  uint8_t rgbFound = 0;
  if (chain.getDeviceList(&deviceList)) {
    for (uint16_t i = 0; i < deviceList.count; ++i) {
      const auto& device = deviceList.devices[i];
      if (device.device_type == CHAIN_RGB_TYPE_CODE && rgbFound < kMatrixCount) {
        rgbDeviceIds[rgbFound++] = device.id;
      }
    }
  }
  free(devices);

  if (rgbFound != kMatrixCount) {
    Serial.printf("Chain RGB: expected %u, found %u\n", kMatrixCount, rgbFound);
    return false;
  }

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

uint8_t stageForRpm(uint16_t rpm) {
  if (rpm >= 6200) return 5;
  if (rpm >= 5600) return 4;
  if (rpm >= 5000) return 3;
  if (rpm >= 4400) return 2;
  if (rpm >= 3800) return 1;
  return 0;
}

uint16_t cornerColorFor(uint16_t color) {
  if (color == kGreen) return kGreenCorner;
  if (color == kYellow) return kYellowCorner;
  return kRedCorner;
}

void fillCentralSection(uint16_t (&frame)[64], uint16_t color) {
  for (uint8_t y = kSectionStart; y < kSectionStart + kSectionSize; ++y) {
    for (uint8_t x = kSectionStart; x < kSectionStart + kSectionSize; ++x) {
      const bool corner = (x == kSectionStart || x == kSectionStart + kSectionSize - 1) &&
                          (y == kSectionStart || y == kSectionStart + kSectionSize - 1);
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

void renderRedline(bool flashOn) {
  if (!redlineModeActive) {
    uint16_t frames[kMatrixCount][64];
    for (auto& frame : frames) {
      for (auto& pixel : frame) {
        pixel = kRed;
      }
    }
    sendFrames(frames);
    redlineModeActive = true;
    redlineFlashOn = true;
  }
  if (flashOn != redlineFlashOn) {
    setAllBrightness(flashOn ? kBrightnessPercent : 0);
    redlineFlashOn = flashOn;
  }
}

void renderStage(uint8_t stage) {
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
  } else if (stage == 3 || stage == 4) {
    const uint16_t colors[kMatrixCount] = {
        stage == 3 ? kYellow : kRed,
        stage == 3 ? kGreen : kRed,
        stage == 3 ? kGreen : kRed,
    };
    if (!wasRedlineMode) {
      chain.setCentralSectionsPipelined(rgbDeviceIds, colors);
      return;
    }
    for (uint8_t matrix = 0; matrix < kMatrixCount; ++matrix) {
      fillCentralSection(frames[matrix], colors[matrix]);
    }
  }
  sendFrames(frames);
}

void renderScreen(bool rpmFresh, uint8_t stage) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);

  if (!rpmFresh) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.drawString("WAITING", M5.Display.width() / 2, 36);
    M5.Display.drawString("FOR CAN", M5.Display.width() / 2, 88);
    return;
  }

  char rpmText[8] = {};
  snprintf(rpmText, sizeof(rpmText), "%u", currentRpm);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  const uint8_t textSize = M5.Display.width() / M5.Display.textWidth(rpmText);
  M5.Display.setTextSize(textSize);
  M5.Display.drawString(rpmText, M5.Display.width() / 2, 45);
  M5.Display.setTextColor(stage >= 4 ? kRed : stage == 3 ? kYellow : kGreen, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString("RPM", M5.Display.width() / 2, 79);
  if (stage == 5) {
    M5.Display.drawString("REDLINE", M5.Display.width() / 2, 104);
  } else {
    char pointText[16] = {};
    snprintf(pointText, sizeof(pointText), "POINT %u/5", stage);
    M5.Display.drawString(pointText, M5.Display.width() / 2, 104);
  }
}

bool readRpmFrames() {
  bool updated = false;
  twai_message_t message = {};
  while (twai_receive(&message, 0) == ESP_OK) {
    if (message.extd || message.identifier != kRpmCanId || message.data_length_code <= kRpmHighByte) {
      continue;
    }
    const uint16_t raw = static_cast<uint16_t>(message.data[kRpmLowByte]) |
                         (static_cast<uint16_t>(message.data[kRpmHighByte]) << 8);
    currentRpm = (raw + 2) / 4;
    lastRpmFrameMs = millis();
    updated = true;
  }
  return updated;
}

}  // namespace

void setup() {
  auto config = M5.config();
  M5.begin(config);
  M5.Display.setRotation(3);
  Serial.begin(115200);
  delay(200);

  if (!initialiseCan()) {
    showFatal("CAN INIT FAILED", "Atomic CAN Base / pins");
    while (true) delay(250);
  }
  if (!initialiseMatrices()) {
    showFatal("CHAIN RGB ERROR", "Need 3 matrices");
    while (true) delay(250);
  }

  renderStage(0);
  renderScreen(false, 0);
  Serial.println("Shiftlight ready; waiting for PT-CAN RPM frame 0x0A5.");
}

void loop() {
  M5.update();
  const bool rpmUpdated = readRpmFrames();
  const uint32_t now = millis();
  const bool rpmFresh = lastRpmFrameMs != 0 && now - lastRpmFrameMs <= kRpmTimeoutMs;
  const uint8_t stage = rpmFresh ? stageForRpm(currentRpm) : 0;

  if (stage != lastRenderedStage || stage == 5) {
    renderStage(stage);
    lastRenderedStage = stage;
  }
  if (rpmUpdated || now - lastScreenRefreshMs >= kScreenRefreshMs) {
    renderScreen(rpmFresh, stage);
    lastScreenRefreshMs = now;
  }
}
