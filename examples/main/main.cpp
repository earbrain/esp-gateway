#include "earbrain/gateway/gateway.hpp"
#include "esp_err.h"
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
  if (gateway.add_route("/api/ext/hello", HTTP_GET, &custom_hello_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register /api/ext/hello");
    return;
  }

  ESP_LOGI(TAG, "Gateway version: %s", gateway.version());

  ESP_LOGI(TAG, "Starting access point");
  earbrain::AccessPointConfig ap_cfg;
  ap_cfg.ssid = "gateway-ap";
  if (gateway.start_access_point(ap_cfg) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start access point");
    return;
  }

  const esp_err_t sta_saved_err = gateway.start_station();
  if (sta_saved_err == ESP_ERR_NOT_FOUND) {
    ESP_LOGI(TAG, "No saved station credentials; AP mode only.");
  } else if (sta_saved_err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to start station with saved credentials: %s",
             esp_err_to_name(sta_saved_err));
  } else {
    ESP_LOGI(TAG, "Station auto-connect started using saved credentials");
  }

  ESP_LOGI(TAG, "Starting HTTP server");
  if (gateway.start_server() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return;
  }

  earbrain::MdnsConfig mdns_cfg;
  mdns_cfg.hostname = "esp-gateway";
  mdns_cfg.instance_name = "ESP Gateway";
  mdns_cfg.service_type = "_http";
  mdns_cfg.protocol = "_tcp";
  mdns_cfg.port = 80;

  const esp_err_t mdns_err = gateway.start_mdns(mdns_cfg);
  if (mdns_err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to start mDNS: %s", esp_err_to_name(mdns_err));
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
