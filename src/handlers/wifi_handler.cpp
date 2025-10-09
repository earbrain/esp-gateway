#include "earbrain/gateway/handlers/wifi_handler.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "earbrain/gateway/gateway.hpp"
#include "earbrain/gateway/logging.hpp"
#include "json/http_response.hpp"
#include "json/json_helpers.hpp"
#include "json/wifi_credentials.hpp"

namespace earbrain::handlers::wifi {

namespace {

constexpr std::size_t max_request_body_size = 1024;

bool is_valid_passphrase(std::string_view passphrase) {
  const std::size_t len = passphrase.size();
  if (len >= 8 && len <= 63) {
    return true;
  }
  if (len == 64) {
    return std::all_of(passphrase.begin(), passphrase.end(), [](char ch) {
      return std::isxdigit(static_cast<unsigned char>(ch));
    });
  }
  return false;
}

} // namespace

esp_err_t handle_credentials_post(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);
  if (!gateway) {
    return http::send_error(req, "Gateway unavailable", "gateway_unavailable");
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

  WifiCredentials credentials{};
  const char *bad_field = nullptr;
  if (!json_model::parse_wifi_credentials(root.get(), credentials, &bad_field)) {
    std::string message = bad_field
                            ? std::string(bad_field) + " must be a string."
                            : "Invalid credentials payload.";
    return http::send_fail_field(req, bad_field ? bad_field : "body",
                                 message.c_str());
  }

  if (credentials.ssid.empty() || credentials.ssid.size() > 32) {
    return http::send_fail_field(req, "ssid",
                                 "ssid must be 1-32 characters.");
  }

  if (!is_valid_passphrase(credentials.passphrase)) {
    return http::send_fail_field(req, "passphrase",
                                 "Passphrase must be 8-63 chars or 64 hex.");
  }

  logging::infof("gateway",
                 "Received Wi-Fi credentials update for SSID='%s' (len=%zu)",
                 credentials.ssid.c_str(), credentials.ssid.size());

  const esp_err_t result =
      gateway->save_wifi_credentials(credentials.ssid, credentials.passphrase);
  if (result != ESP_OK) {
    logging::errorf("gateway", "Failed to save Wi-Fi credentials: %s",
                    esp_err_to_name(result));
    return http::send_error(req, "Failed to save credentials.",
                            esp_err_to_name(result));
  }

  logging::info("Wi-Fi credentials saved. Preparing to start station",
                "gateway");

  const esp_err_t stop_err = gateway->stop_station();
  if (stop_err != ESP_OK) {
    logging::warnf("gateway", "Failed to stop existing station: %s",
                   esp_err_to_name(stop_err));
  }

  StationConfig station_cfg{};
  station_cfg.ssid = credentials.ssid;
  station_cfg.passphrase = credentials.passphrase;

  const esp_err_t sta_err = gateway->start_station(station_cfg);
  const bool sta_started = (sta_err == ESP_OK);
  if (sta_started) {
    logging::infof("gateway", "Station connection initiated for SSID: %s",
                   station_cfg.ssid.c_str());
  } else {
    logging::errorf("gateway", "Failed to start station: %s",
                    esp_err_to_name(sta_err));
  }
  gateway->wifi().set_autoconnect_attempted(true);

  auto data = json::object();
  if (!data) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "restart_required", !sta_started) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "sta_connect_started", sta_started) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (!sta_started &&
      json::add(data.get(), "sta_error", esp_err_to_name(sta_err)) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::wifi
