#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace earbrain::handlers::wifi {

esp_err_t handle_credentials_post(httpd_req_t *req);
esp_err_t handle_status_get(httpd_req_t *req);
esp_err_t handle_scan_get(httpd_req_t *req);

} // namespace earbrain::handlers::wifi
