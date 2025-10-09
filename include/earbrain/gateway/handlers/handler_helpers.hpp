#pragma once

#include "esp_http_server.h"
#include "earbrain/gateway/gateway.hpp"
#include "json/http_response.hpp"

namespace earbrain::handlers {

inline Gateway* get_gateway(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);
  if (!gateway) {
    http::send_error(req, "Gateway unavailable", "gateway_unavailable");
  }
  return gateway;
}

} // namespace earbrain::handlers
