#include "earbrain/gateway/handlers/wifi_handler.hpp"

#include "esp_wifi.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/handlers/handler_helpers.hpp"
#include "earbrain/logging.hpp"
#include "earbrain/validation.hpp"
#include "earbrain/wifi_service.hpp"
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

  WifiCredentials wifi_creds{};
  const char *bad_field = nullptr;
  if (!json_model::parse_wifi_credentials(root.get(), wifi_creds, &bad_field)) {
    std::string message = bad_field
                            ? std::string(bad_field) + " must be a string."
                            : "Invalid credentials payload.";
    return http::send_fail_field(req, bad_field ? bad_field : "body",
                                 message.c_str());
  }

  if (!validation::is_valid_ssid(wifi_creds.ssid)) {
    return http::send_fail_field(req, "ssid",
                                 "ssid must be 1-32 characters.");
  }

  if (!validation::is_valid_passphrase(wifi_creds.passphrase)) {
    return http::send_fail_field(req, "passphrase",
                                 "Passphrase must be 8-63 chars or 64 hex.");
  }

  logging::infof("gateway",
                 "Received Wi-Fi credentials update for SSID='%s' (len=%zu)",
                 wifi_creds.ssid.c_str(), wifi_creds.ssid.size());

  const esp_err_t result = earbrain::wifi().save_credentials(wifi_creds.ssid, wifi_creds.passphrase);
  if (result != ESP_OK) {
    logging::errorf("gateway", "Failed to save Wi-Fi credentials: %s",
                    esp_err_to_name(result));
    return http::send_error(req, "Failed to save credentials.",
                            esp_err_to_name(result));
  }

  logging::info("Wi-Fi credentials saved successfully", "gateway");

  // Emit credentials saved event
  gateway->emit(Gateway::Event::WifiCredentialsSaved, wifi_creds);

  return http::send_success(req);
}

esp_err_t handle_connect_post(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  logging::info("Attempting to connect using saved credentials", "gateway");

  // Get saved credentials to check if they exist
  auto saved_creds = earbrain::wifi().load_credentials();
  if (!saved_creds.has_value()) {
    return http::send_error(req, "No saved credentials found", "ESP_ERR_NOT_FOUND");
  }

  const esp_err_t result = earbrain::wifi().connect();
  if (result != ESP_OK) {
    logging::errorf("gateway", "Failed to initiate connection: %s", esp_err_to_name(result));

    const char* error_msg = "Failed to initiate connection";
    if (result == ESP_ERR_INVALID_STATE) {
      error_msg = "WiFi not in correct mode (APSTA required)";
    }

    return http::send_error(req, error_msg, esp_err_to_name(result));
  }

  logging::info("Connection initiated, check /api/v1/wifi/status for progress", "gateway");

  // Return success immediately - client should poll /api/v1/wifi/status to check connection status
  return http::send_success(req);
}

esp_err_t handle_status_get(httpd_req_t *req) {
  auto *gateway = handlers::get_gateway(req);
  if (!gateway) {
    return ESP_FAIL;
  }

  earbrain::WifiStatus wifi_status = earbrain::wifi().status();

  json_model::WifiStatus status;
  // Map WifiMode to ap_active and sta_active for backward compatibility
  status.ap_active = (wifi_status.mode == WifiMode::AP || wifi_status.mode == WifiMode::APSTA);
  status.sta_active = (wifi_status.mode == WifiMode::STA || wifi_status.mode == WifiMode::APSTA);
  status.sta_connected = wifi_status.sta_connected;
  status.sta_connecting = wifi_status.sta_connecting;
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

  WifiScanResult result = earbrain::wifi().perform_scan();

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
