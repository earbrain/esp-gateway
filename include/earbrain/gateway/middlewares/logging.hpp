#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace earbrain::middleware {

esp_err_t log_request(httpd_req_t *req);

} // namespace earbrain::middleware
