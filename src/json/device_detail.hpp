#pragma once

#include "earbrain/gateway/device_detail.hpp"
#include "json/json_helpers.hpp"

namespace earbrain::json_model {

inline json::Ptr to_json(const DeviceDetail &detail) {
  auto obj = json::object();
  if (!obj) {
    return obj;
  }

  if (json::add(obj.get(), "model", detail.model) != ESP_OK) {
    return json::Ptr{};
  }
  if (json::add(obj.get(), "gateway_version", detail.gateway_version) !=
      ESP_OK) {
    return json::Ptr{};
  }
  if (json::add(obj.get(), "build_time", detail.build_time) != ESP_OK) {
    return json::Ptr{};
  }
  if (json::add(obj.get(), "idf_version", detail.idf_version) != ESP_OK) {
    return json::Ptr{};
  }

  return obj;
}

} // namespace earbrain::json_model
