#pragma once

#include "esp_http_server.h"

namespace earbrain::handlers::health {

// health check endpoint
esp_err_t handle_health(httpd_req_t *req);

} // namespace earbrain::handlers::health
