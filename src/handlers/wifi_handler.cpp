#include "earbrain/gateway/handlers/wifi_handler.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "earbrain/gateway/logging.hpp"
#include "earbrain/gateway/task_helpers.hpp"
#include "earbrain/gateway/validation.hpp"
#include "json/http_response.hpp"
#include "json/json_helpers.hpp"
#include "json/wifi_credentials.hpp"
#include "json/wifi_status.hpp"
#include "json/wifi_scan.hpp"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace earbrain::handlers::wifi {

namespace {

constexpr std::size_t max_request_body_size = 1024;

} // namespace

esp_err_t handle_credentials_post(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  if (req->content_len <= 0 ||
      static_cast<std::size_t>(req->content_len) > max_request_body_size) {
    return http::send_fail(req, "Invalid request size.");
  }

  std::string body;
  body.resize(static_cast<std::size_t>(req->content_len));
  std::size_t received = 0;

  while (received < body.size()) {
    const int ret =
        httpd_req_recv(req, body.data() + received, body.size() - received);
    if (ret <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      return http::send_fail(req, "Failed to read request body.");
    }
    received += static_cast<std::size_t>(ret);
  }

  auto root = json::parse(body);
  if (!root || !cJSON_IsObject(root.get())) {
    return http::send_fail(req, "Invalid JSON body.");
  }

  StationConfig station_cfg{};
  const char *bad_field = nullptr;
  if (!json_model::parse_station_config(root.get(), station_cfg, &bad_field)) {
    std::string message = bad_field
                            ? std::string(bad_field) + " must be a string."
                            : "Invalid credentials payload.";
    return http::send_fail_field(req, bad_field ? bad_field : "body",
                                 message.c_str());
  }

  if (!validation::is_valid_ssid(station_cfg.ssid)) {
    return http::send_fail_field(req, "ssid",
                                 "ssid must be 1-32 characters.");
  }

  if (!validation::is_valid_passphrase(station_cfg.passphrase)) {
    return http::send_fail_field(req, "passphrase",
                                 "Passphrase must be 8-63 chars or 64 hex.");
  }

  logging::infof("gateway",
                 "Received Wi-Fi credentials update for SSID='%s' (len=%zu)",
                 station_cfg.ssid.c_str(), station_cfg.ssid.size());

  const esp_err_t result =
      gateway->save_wifi_credentials(station_cfg.ssid, station_cfg.passphrase);
  if (result != ESP_OK) {
    logging::errorf("gateway", "Failed to save Wi-Fi credentials: %s",
                    esp_err_to_name(result));
    return http::send_error(req, "Failed to save credentials.",
                            esp_err_to_name(result));
  }

  logging::info("Wi-Fi credentials saved successfully", "gateway");

  // Start connection in background task
  const esp_err_t task_err = tasks::run_detached([gateway, station_cfg]() {
    // Small delay to ensure HTTP response is sent
    vTaskDelay(pdMS_TO_TICKS(100));

    logging::infof("gateway", "Starting Wi-Fi connection for SSID: %s", station_cfg.ssid.c_str());

    const esp_err_t stop_err = gateway->wifi().stop_station();
    if (stop_err != ESP_OK) {
      logging::warnf("gateway", "Failed to stop existing station: %s", esp_err_to_name(stop_err));
    }

    const esp_err_t sta_err = gateway->wifi().start_station(station_cfg);
    if (sta_err == ESP_OK) {
      logging::infof("gateway", "Station connection initiated for SSID: %s", station_cfg.ssid.c_str());
    } else {
      logging::errorf("gateway", "Failed to start station: %s", esp_err_to_name(sta_err));
    }
    gateway->wifi().set_autoconnect_attempted(true);
  }, "wifi_connect");

  if (task_err != ESP_OK) {
    logging::error("gateway", "Failed to create Wi-Fi connect task");
    return http::send_error(req, "Failed to start connection task.", "ESP_FAIL");
  }

  return http::send_success(req);
}

esp_err_t handle_status_get(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  WifiStatus wifi_status = gateway->wifi().status();

  json_model::WifiStatus status;
  status.ap_active = wifi_status.ap_active;
  status.sta_active = wifi_status.sta_active;
  status.sta_connecting = wifi_status.sta_connecting;
  status.sta_connected = wifi_status.sta_connected;
  status.last_error = wifi_status.sta_last_error;
  status.disconnect_reason = wifi_status.sta_last_disconnect_reason;
  status.connection_type = handlers::get_connection_type(req, gateway);

  const ip4_addr_t *ip4 = reinterpret_cast<const ip4_addr_t *>(&wifi_status.sta_ip);
  char ip_buffer[16] = {0};
  if (wifi_status.sta_connected && ip4addr_ntoa_r(ip4, ip_buffer, sizeof(ip_buffer))) {
    status.ip = ip_buffer;
  }

  auto data = json_model::to_json(status);
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

esp_err_t handle_scan_get(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  WifiScanResult result = gateway->wifi().perform_scan();

  if (result.error != ESP_OK) {
    return http::send_error(req, "Wi-Fi scan failed", esp_err_to_name(result.error));
  }

  auto payload = json_model::to_json(result);
  if (!payload) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(payload));
}

} // namespace earbrain::handlers::wifi
