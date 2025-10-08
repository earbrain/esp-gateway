#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace earbrain::handlers::portal {

esp_err_t handle_root_get(httpd_req_t *req);
esp_err_t handle_app_js_get(httpd_req_t *req);
esp_err_t handle_assets_css_get(httpd_req_t *req);

} // namespace earbrain::handlers::portal

