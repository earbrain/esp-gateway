#pragma once

#include "esp_http_server.h"
#include "earbrain/gateway/gateway.hpp"
#include "earbrain/wifi_service.hpp"
#include "json/http_response.hpp"
#include <string>

namespace earbrain::handlers {

inline std::string get_connection_type(httpd_req_t *req, Gateway *gateway) {
  if (!req || !gateway) {
    return "unknown";
  }

  WifiStatus status = earbrain::wifi().status();

  // Check WiFi mode and determine connection type
  if (status.mode == WifiMode::STA || status.mode == WifiMode::APSTA) {
    return "sta";
  }

  if (status.mode == WifiMode::AP) {
    return "ap";
  }

  return "unknown";
}

} // namespace earbrain::handlers
