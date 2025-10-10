#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/middlewares/logging.hpp"
#include "esp_err.h"
#include "earbrain/gateway/logging.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Simple custom middleware that adds a custom header
static esp_err_t add_custom_header(httpd_req_t *req, earbrain::NextHandler next) {
  httpd_resp_set_hdr(req, "X-Custom", "HelloWorld");

  // Call the next handler
  esp_err_t result = next(req);

  // Can add post-processing here if needed
  return result;
}

static esp_err_t custom_hello_handler(httpd_req_t *req) {
  static constexpr char payload[] = R"({"message":"hello"})";
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

extern "C" void app_main(void) {
  static const char *TAG = "gateway_example";

  earbrain::Gateway gateway;

  // Apply logging middleware globally to all routes
  gateway.server().use(earbrain::middleware::log_request);

  // Add custom route with route-specific middleware
  earbrain::RouteOptions hello_opts;
  hello_opts.middlewares = {add_custom_header};
  if (gateway.add_route("/api/ext/hello", HTTP_GET, &custom_hello_handler, hello_opts) != ESP_OK) {
    earbrain::logging::error("Failed to register /api/ext/hello", TAG);
    return;
  }

  earbrain::logging::infof(TAG, "Gateway version: %s", gateway.version());

  earbrain::logging::info("Starting access point", TAG);
  earbrain::AccessPointConfig ap_cfg;
  ap_cfg.ssid = "gateway-ap";
  if (gateway.start_access_point(ap_cfg) != ESP_OK) {
    earbrain::logging::error("Failed to start access point", TAG);
    return;
  }

  // Attempt to connect to saved Wi-Fi credentials
  const esp_err_t sta_err = gateway.start_station();
  if (sta_err == ESP_ERR_NOT_FOUND) {
    earbrain::logging::info("No saved Wi-Fi credentials found", TAG);
  } else if (sta_err != ESP_OK) {
    earbrain::logging::warnf(TAG, "Failed to connect to saved Wi-Fi: %s",
                             esp_err_to_name(sta_err));
  } else {
    earbrain::logging::info("Attempting to connect to saved Wi-Fi network", TAG);
  }

  earbrain::logging::info("Starting HTTP server", TAG);
  if (gateway.start_server() != ESP_OK) {
    earbrain::logging::error("Failed to start HTTP server", TAG);
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
    earbrain::logging::warnf(TAG, "Failed to start mDNS: %s", esp_err_to_name(mdns_err));
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
