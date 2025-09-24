#include "earbrain/gateway/gateway.hpp"
#include "esp_log.h"

extern "C" void app_main(void) {
  static const char *TAG = "gateway_example";

  earbrain::Gateway gateway;

  ESP_LOGI(TAG, "Gateway start");
  gateway.start();

  ESP_LOGI(TAG, "Gateway stop");
  gateway.stop();
}
