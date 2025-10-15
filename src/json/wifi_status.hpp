#pragma once

#include "json/json_helpers.hpp"

#include "esp_err.h"
#include "esp_wifi_types.h"

#include <string>

namespace earbrain::json_model {

struct WifiStatus {
  bool ap_active = false;
  bool sta_active = false;
  bool sta_connected = false;
  bool sta_connecting = false;
  std::string ip;
  wifi_err_reason_t disconnect_reason = WIFI_REASON_UNSPECIFIED;
  esp_err_t last_error = ESP_OK;
};

inline std::string map_wifi_error_to_message(esp_err_t err) {
  switch (err) {
  case ESP_OK:
    return "";
  case ESP_ERR_WIFI_PASSWORD:
    return "Authentication failed (wrong password?)";
  case ESP_ERR_WIFI_SSID:
    return "Network not found";
  case ESP_ERR_TIMEOUT:
    return "Connection timeout";
  case ESP_ERR_INVALID_STATE:
    return "WiFi not in correct mode (APSTA required)";
  default:
    return esp_err_to_name(err);
  }
}

inline json::Ptr to_json(const WifiStatus &status) {
  auto obj = json::object();
  if (!obj) {
    return nullptr;
  }

  auto add_bool = [&](const char *key, bool value) -> esp_err_t {
    return json::add(obj.get(), key, value);
  };

  if (add_bool("ap_active", status.ap_active) != ESP_OK) {
    return nullptr;
  }
  if (add_bool("sta_active", status.sta_active) != ESP_OK) {
    return nullptr;
  }
  if (add_bool("sta_connected", status.sta_connected) != ESP_OK) {
    return nullptr;
  }
  if (add_bool("sta_connecting", status.sta_connecting) != ESP_OK) {
    return nullptr;
  }

  const std::string error_message = map_wifi_error_to_message(status.last_error);
  if (json::add(obj.get(), "sta_error", error_message.c_str()) != ESP_OK) {
    return nullptr;
  }

  if (json::add(obj.get(), "ip", status.ip) != ESP_OK) {
    return nullptr;
  }

  if (!cJSON_AddNumberToObject(obj.get(), "disconnect_reason",
                               static_cast<int>(status.disconnect_reason))) {
    return nullptr;
  }

  return obj;
}

} // namespace earbrain::json_model
