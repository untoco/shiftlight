#include <M5Chain.h>
#include <M5Unified.h>

namespace {

constexpr gpio_num_t kChainRxPin = GPIO_NUM_1;
constexpr gpio_num_t kChainTxPin = GPIO_NUM_2;
constexpr uint32_t kChainBaudRate = 115200;
constexpr uint8_t kBrightnessPercent = 25;
constexpr uint32_t kPhaseDurationMs = 750;
constexpr uint16_t kGreen = 0x07E0;

Chain chain;
uint8_t rgbDeviceId = 0;
uint8_t operationStatus = 0;
bool matrixIsOn = false;
uint32_t lastPhaseChangeMs = 0;

void showStatus(const char* headline, const char* detail, uint16_t color) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(headline, M5.Display.width() / 2, 36);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(detail, M5.Display.width() / 2, 72);
}

bool findChainRgb() {
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
  bool found = false;
  if (chain.getDeviceList(&deviceList)) {
    for (uint16_t i = 0; i < deviceList.count; ++i) {
      Serial.printf("Chain device: id=%u, type=0x%04X\n", deviceList.devices[i].id,
                    deviceList.devices[i].device_type);
      if (deviceList.devices[i].device_type == CHAIN_RGB_TYPE_CODE) {
        rgbDeviceId = deviceList.devices[i].id;
        found = true;
        break;
      }
    }
  }
  free(devices);

  if (!found) {
    Serial.println("No Chain RGB matrix found.");
  }
  return found;
}

bool initialiseMatrix() {
  if (!findChainRgb()) {
    return false;
  }

  if (chain.setRGBMode(rgbDeviceId, RGB_PIXEL_MODE, &operationStatus) != CHAIN_OK ||
      operationStatus != 1) {
    Serial.println("Could not enable Chain RGB pixel mode.");
    return false;
  }

  chain.setRGBRotation(rgbDeviceId, RGB_ROTATION_0, &operationStatus);
  chain.setRGBBrightness(rgbDeviceId, kBrightnessPercent, &operationStatus);
  chain.setRGBClear(rgbDeviceId, &operationStatus);
  Serial.printf("Chain RGB ready: id=%u, brightness=%u%%\n", rgbDeviceId,
                kBrightnessPercent);
  return operationStatus == 1;
}

void setMatrix(bool on) {
  if (on) {
    uint16_t frame[64];
    for (auto& pixel : frame) {
      pixel = kGreen;
    }
    chain.setRGBBufferRefresh(rgbDeviceId, frame, &operationStatus);
  } else {
    chain.setRGBClear(rgbDeviceId, &operationStatus);
  }

  Serial.printf("Matrix %s (result=%u)\n", on ? "ON" : "OFF", operationStatus);
  showStatus(on ? "CHAIN RGB ON" : "CHAIN RGB OFF", on ? "GREEN / 25%" : "750 ms cycle",
             on ? kGreen : TFT_RED);
}

}  // namespace

void setup() {
  auto config = M5.config();
  M5.begin(config);
  Serial.begin(115200);
  delay(200);

  showStatus("SHIFT LIGHT", "Searching Chain RGB...", TFT_YELLOW);
  Serial.println("Shiftlight Chain RGB blink test");
  Serial.printf("Chain UART: RX=GPIO%d, TX=GPIO%d, %lu baud\n", kChainRxPin, kChainTxPin,
                kChainBaudRate);

  chain.begin(&Serial2, kChainBaudRate, kChainRxPin, kChainTxPin);
  if (!initialiseMatrix()) {
    showStatus("NO CHAIN RGB", "Check IN port and cable", TFT_RED);
    while (true) {
      delay(250);
    }
  }

  showStatus("CHAIN RGB READY", "Blink test starts", kGreen);
  delay(500);
  setMatrix(true);
  matrixIsOn = true;
  lastPhaseChangeMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastPhaseChangeMs < kPhaseDurationMs) {
    return;
  }

  matrixIsOn = !matrixIsOn;
  setMatrix(matrixIsOn);
  lastPhaseChangeMs = now;
}
