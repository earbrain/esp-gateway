#include "earbrain/gateway/gateway.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static esp_err_t custom_hello_handler(httpd_req_t *req) {
  static constexpr char payload[] = R"({"message":"hello"})";
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

extern "C" void app_main(void) {
  static const char *TAG = "gateway_example";

  earbrain::Gateway gateway;

  // Register user custom API
  if (gateway.get("/api/ext/hello", &custom_hello_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register /api/ext/hello");
    return;
  }

  ESP_LOGI(TAG, "Gateway version: %s", gateway.version());

  ESP_LOGI(TAG, "Gateway start");
  if (gateway.start() != ESP_OK) {
    ESP_LOGE(TAG, "Gateway start failed");
    return;
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
