#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace earbrain::handlers::metrics {

esp_err_t handle_get(httpd_req_t *req);

} // namespace earbrain::handlers::metrics

