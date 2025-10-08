#pragma once

#include "json/json_helpers.hpp"
#include "earbrain/gateway/wifi_scan.hpp"

namespace earbrain::json_model {

inline std::string auth_mode_to_string(wifi_auth_mode_t mode) {
  switch (mode) {
  case WIFI_AUTH_OPEN:
    return "Open";
  case WIFI_AUTH_WEP:
    return "WEP";
  case WIFI_AUTH_WPA_PSK:
    return "WPA";
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2";
  case WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA/WPA2";
  case WIFI_AUTH_WPA2_ENTERPRISE:
    return "WPA2-Enterprise";
  case WIFI_AUTH_WPA3_PSK:
    return "WPA3";
  case WIFI_AUTH_WPA2_WPA3_PSK:
    return "WPA2/WPA3";
  case WIFI_AUTH_WAPI_PSK:
    return "WAPI";
  case WIFI_AUTH_OWE:
    return "OWE";
  default:
    return "Unknown";
  }
}

inline json::Ptr to_json(const WifiScanResult &result) {
  auto root = json::object();
  if (!root) {
    return nullptr;
  }

  cJSON *items = cJSON_AddArrayToObject(root.get(), "networks");
  if (!items) {
    return nullptr;
  }

  for (const auto &network : result.networks) {
    cJSON *entry = cJSON_CreateObject();
    if (!entry) {
      return nullptr;
    }
    if (json::add(entry, "ssid", network.ssid) != ESP_OK) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (json::add(entry, "bssid", network.bssid) != ESP_OK) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (!cJSON_AddNumberToObject(entry, "rssi", network.rssi)) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (!cJSON_AddNumberToObject(entry, "signal", network.signal)) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (!cJSON_AddNumberToObject(entry, "channel", network.channel)) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (json::add(entry, "security", auth_mode_to_string(network.auth_mode)) != ESP_OK) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (json::add(entry, "connected", network.connected) != ESP_OK) {
      cJSON_Delete(entry);
      return nullptr;
    }
    if (json::add(entry, "hidden", network.hidden) != ESP_OK) {
      cJSON_Delete(entry);
      return nullptr;
    }
    cJSON_AddItemToArray(items, entry);
  }

  const char *error_name = result.error == ESP_OK
                             ? ""
                             : esp_err_to_name(result.error);
  if (json::add(root.get(), "error", error_name) != ESP_OK) {
    return nullptr;
  }

  return root;
}

} // namespace earbrain::json_model
