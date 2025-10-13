#pragma once

#include "json/json_helpers.hpp"
#include "earbrain/gateway/wifi_service.hpp"

#include <string>
#include <utility>

namespace earbrain::json_model {

inline bool parse_station_config(cJSON *root, StationConfig &out,
                                 const char **bad_field) {
  if (!root) {
    if (bad_field) {
      *bad_field = nullptr;
    }
    return false;
  }

  std::string ssid;
  if (!json::required_string(root, "ssid", ssid)) {
    if (bad_field) {
      *bad_field = "ssid";
    }
    return false;
  }

  std::string passphrase;
  if (!json::required_string(root, "passphrase", passphrase)) {
    if (bad_field) {
      *bad_field = "passphrase";
    }
    return false;
  }

  out.ssid = std::move(ssid);
  out.passphrase = std::move(passphrase);
  if (bad_field) {
    *bad_field = nullptr;
  }
  return true;
}

} // namespace earbrain::json_model
