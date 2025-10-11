#pragma once

#include "esp_http_server.h"
#include "earbrain/gateway/gateway.hpp"
#include "json/http_response.hpp"
#include <string>

namespace earbrain::handlers {

inline Gateway* get_gateway(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);
  if (!gateway) {
    http::send_error(req, "Gateway unavailable", "gateway_unavailable");
  }
  return gateway;
}

inline std::string get_connection_type(httpd_req_t *req, Gateway *gateway) {
  if (!req || !gateway) {
    return "unknown";
  }

  WifiStatus status = gateway->wifi().status();

  // Both AP and STA are active and connected
  if (status.ap_active && status.sta_active && status.sta_connected) {
    return "apsta";
  }

  // Only STA is active and connected
  if (status.sta_active && status.sta_connected) {
    return "sta";
  }

  // Only AP is active (or STA not connected)
  if (status.ap_active) {
    return "ap";
  }

  return "unknown";
}

} // namespace earbrain::handlers
