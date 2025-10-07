#pragma once

#include "json/json_helpers.hpp"

#include "esp_err.h"
#include "esp_wifi_types.h"

#include <string>

namespace earbrain {

struct WifiStatus {
  bool ap_active = false;
  bool sta_active = false;
  bool sta_connecting = false;
  bool sta_connected = false;
  std::string ip;
  wifi_err_reason_t disconnect_reason = WIFI_REASON_UNSPECIFIED;
  esp_err_t last_error = ESP_OK;
};

} // namespace earbrain

namespace earbrain::json_model {

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
  if (add_bool("sta_connecting", status.sta_connecting) != ESP_OK) {
    return nullptr;
  }
  if (add_bool("sta_connected", status.sta_connected) != ESP_OK) {
    return nullptr;
  }

  const char *error_name = status.last_error == ESP_OK
                             ? ""
                             : esp_err_to_name(status.last_error);
  if (json::add(obj.get(), "sta_error", error_name) != ESP_OK) {
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

