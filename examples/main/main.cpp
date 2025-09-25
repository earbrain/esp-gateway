#include "earbrain/gateway/gateway.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
  static const char *TAG = "gateway_example";

  earbrain::Gateway gateway;

  ESP_LOGI(TAG, "Gateway version: %s", gateway.version());

  ESP_LOGI(TAG, "Gateway start");
  gateway.start();

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
