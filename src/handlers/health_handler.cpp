#include "earbrain/gateway/handlers/health_handler.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "earbrain/gateway/gateway.hpp"
#include "json/json_helpers.hpp"
#include "json/http_response.hpp"
#include "esp_timer.h"

namespace earbrain::handlers::health {

esp_err_t handle_health(httpd_req_t *req) {

  auto data = json::object();
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  // Basic status
  if (json::add(data.get(), "status", "ok") != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  // Uptime in seconds
  int64_t uptime_us = esp_timer_get_time();
  if (json::add(data.get(), "uptime", static_cast<int>(uptime_us / 1000000)) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  // Version
  if (json::add(data.get(), "version", gateway().version()) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::health
