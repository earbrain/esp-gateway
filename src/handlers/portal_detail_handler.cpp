#include "earbrain/gateway/handlers/portal_detail_handler.hpp"

#include <utility>

#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "json/http_response.hpp"
#include "json/json_helpers.hpp"
#include "json/portal_detail.hpp"

namespace earbrain::handlers::portal_detail {

esp_err_t handle_get(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  json_model::PortalDetail detail;
  detail.title = gateway->get_portal_title();

  auto data = json_model::to_json(detail);
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::portal_detail
