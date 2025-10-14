#include "earbrain/gateway/handlers/metrics_handler.hpp"

#include <utility>

#include "earbrain/metrics.hpp"
#include "json/http_response.hpp"
#include "json/metrics.hpp"

namespace earbrain::handlers::metrics {

esp_err_t handle_get(httpd_req_t *req) {
  const Metrics metrics = collect_metrics();

  auto data = json_model::to_json(metrics);
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::metrics
