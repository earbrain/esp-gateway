#include "earbrain/gateway/handlers/device_handler.hpp"

#include <utility>

#include "earbrain/gateway/device_detail.hpp"
#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "json/device_detail.hpp"
#include "json/http_response.hpp"
#include "json/json_helpers.hpp"
#include "esp_chip_info.h"
#include "esp_system.h"

namespace earbrain::handlers::device {

namespace {

const char *chip_model_string(const esp_chip_info_t &info) {
  switch (info.model) {
  case CHIP_ESP32:
    return "ESP32";
  case CHIP_ESP32S2:
    return "ESP32-S2";
  case CHIP_ESP32S3:
    return "ESP32-S3";
  case CHIP_ESP32C3:
    return "ESP32-C3";
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  case CHIP_ESP32C2:
    return "ESP32-C2";
  case CHIP_ESP32C6:
    return "ESP32-C6";
#endif
  case CHIP_ESP32H2:
    return "ESP32-H2";
  default:
    return "Unknown";
  }
}

constexpr const char build_timestamp[] = __DATE__ " " __TIME__;

} // namespace

esp_err_t handle_get(httpd_req_t *req) {

  esp_chip_info_t chip_info{};
  esp_chip_info(&chip_info);

  DeviceDetail detail;
  detail.model = chip_model_string(chip_info);
  detail.gateway_version = gateway().version();
  detail.build_time = build_timestamp;
  detail.idf_version = esp_get_idf_version();

  auto data = json_model::to_json(detail);
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::device
