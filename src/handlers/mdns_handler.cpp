#include "earbrain/gateway/handlers/mdns_handler.hpp"

#include <utility>

#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "json/http_response.hpp"
#include "json/json_helpers.hpp"

namespace earbrain::handlers::mdns {

esp_err_t handle_get(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  auto data = json::object();
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  const MdnsConfig &config = gateway->mdns().config();

  if (json::add(data.get(), "hostname", config.hostname) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "instance_name", config.instance_name) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "service_type", config.service_type) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "protocol", config.protocol) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (!cJSON_AddNumberToObject(data.get(), "port",
                               static_cast<int>(config.port))) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "running", gateway->mdns().is_running()) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::mdns
