#pragma once

#include "earbrain/gateway/device_info.hpp"
#include "json/json_helpers.hpp"

namespace earbrain::json_model {

inline json::Ptr to_json(const DeviceInfo &info) {
  auto obj = json::object();
  if (!obj) {
    return obj;
  }

  if (json::add(obj.get(), "model", info.model) != ESP_OK) {
    return json::Ptr{};
  }
  if (json::add(obj.get(), "firmware_version", info.firmware_version) !=
      ESP_OK) {
    return json::Ptr{};
  }
  if (json::add(obj.get(), "build_time", info.build_time) != ESP_OK) {
    return json::Ptr{};
  }
  if (json::add(obj.get(), "idf_version", info.idf_version) != ESP_OK) {
    return json::Ptr{};
  }

  return obj;
}

} // namespace earbrain::json_model
