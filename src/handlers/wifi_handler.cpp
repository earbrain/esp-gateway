#include "earbrain/gateway/handlers/wifi_handler.hpp"

#include "esp_wifi.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "earbrain/gateway/logging.hpp"
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
      gateway->wifi().credentials().save(station_cfg.ssid, station_cfg.passphrase);
  if (result != ESP_OK) {
    logging::errorf("gateway", "Failed to save Wi-Fi credentials: %s",
                    esp_err_to_name(result));
    return http::send_error(req, "Failed to save credentials.",
                            esp_err_to_name(result));
  }

  logging::info("Wi-Fi credentials saved successfully", "gateway");
  return http::send_success(req);
}

esp_err_t handle_connect_post(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  logging::info("Attempting to connect using saved credentials", "gateway");

  const esp_err_t result = gateway->wifi().connect();
  if (result == ESP_OK) {
    logging::info("Successfully connected to saved network", "gateway");
    return http::send_success(req);
  }

  // Map error codes to user-friendly messages
  const char* error_msg = "Connection failed";
  switch (result) {
    case ESP_ERR_NOT_FOUND:
      error_msg = "No saved credentials found";
      break;
    case ESP_ERR_WIFI_PASSWORD:
      error_msg = "Authentication failed (wrong password?)";
      break;
    case ESP_ERR_WIFI_SSID:
      error_msg = "Network not found";
      break;
    case ESP_ERR_TIMEOUT:
      error_msg = "Connection timeout";
      break;
    case ESP_ERR_INVALID_STATE:
      error_msg = "WiFi not in correct mode (APSTA required)";
      break;
    default:
      break;
  }

  logging::errorf("gateway", "Connection failed: %s", esp_err_to_name(result));
  return http::send_error(req, error_msg, esp_err_to_name(result));
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
