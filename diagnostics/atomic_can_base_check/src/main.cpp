#include <M5Unified.h>
#include "driver/twai.h"

namespace {

constexpr gpio_num_t kCanTxPin = GPIO_NUM_5;
constexpr gpio_num_t kCanRxPin = GPIO_NUM_6;
constexpr uint32_t kStatusUpdateMs = 1000;

bool canControllerReady = false;
uint32_t lastStatusUpdateMs = 0;

void showStatus(const char* title, const char* detail, uint16_t color) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString(title, M5.Display.width() / 2, 30);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString(detail, M5.Display.width() / 2, 58);
  M5.Display.drawString("GPIO 5 TX / 6 RX", M5.Display.width() / 2, 78);
}

void updateStatus() {
  twai_status_info_t status = {};
  if (twai_get_status_info(&status) != ESP_OK) {
    showStatus("CAN ERROR", "Status unavailable", TFT_RED);
    Serial.println("CAN status read failed");
    return;
  }

  char detail[48] = {};
  snprintf(detail, sizeof(detail), "RX %lu  errors %lu", static_cast<unsigned long>(status.msgs_to_rx),
           static_cast<unsigned long>(status.rx_error_counter));
  showStatus("CAN READY", detail, TFT_GREEN);
  Serial.printf("CAN ready: state=%d, queued_rx=%lu, rx_errors=%lu, tx_errors=%lu\n", status.state,
                static_cast<unsigned long>(status.msgs_to_rx),
                static_cast<unsigned long>(status.rx_error_counter),
                static_cast<unsigned long>(status.tx_error_counter));
}

}  // namespace

void setup() {
  auto config = M5.config();
  M5.begin(config);
  Serial.begin(115200);
  delay(200);

  // This mode never acknowledges or transmits a CAN frame.
  twai_general_config_t general =
      TWAI_GENERAL_CONFIG_DEFAULT(kCanTxPin, kCanRxPin, TWAI_MODE_LISTEN_ONLY);
  twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  const esp_err_t installResult = twai_driver_install(&general, &timing, &filter);
  const esp_err_t startResult = installResult == ESP_OK ? twai_start() : installResult;
  canControllerReady = startResult == ESP_OK;

  if (!canControllerReady) {
    char detail[40] = {};
    snprintf(detail, sizeof(detail), "TWAI error: %s", esp_err_to_name(startResult));
    showStatus("CAN INIT FAILED", detail, TFT_RED);
    Serial.printf("CAN initialisation failed: %s\n", esp_err_to_name(startResult));
    return;
  }

  Serial.println("Atomic CAN Base check started in listen-only mode.");
  Serial.println("TWAI is ready on GPIO 5 (TX) and GPIO 6 (RX).");
  Serial.println("No CAN frames are transmitted by this diagnostic.");
  updateStatus();
}

void loop() {
  M5.update();
  if (!canControllerReady || millis() - lastStatusUpdateMs < kStatusUpdateMs) {
    return;
  }
  lastStatusUpdateMs = millis();
  updateStatus();
}
