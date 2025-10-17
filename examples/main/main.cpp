#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/middlewares/logging.hpp"
#include "esp_err.h"
#include "earbrain/logging.hpp"
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

  // Configure gateway options
  earbrain::GatewayOptions options;
  options.ap_config.ssid = "gateway-ap";
  options.mdns_config.hostname = "esp-gateway";
  options.mdns_config.instance_name = "ESP Gateway";
  options.portal_title = "ESP Gateway Example";

  // Create gateway with options
  earbrain::Gateway gateway(options);

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

  // Start portal (AP + HTTP server + mDNS)
  if (gateway.start_portal() != ESP_OK) {
    earbrain::logging::error("Failed to start portal", TAG);
    return;
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
