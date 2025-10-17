#pragma once

#include "json/json_helpers.hpp"

namespace earbrain::json_model {

struct PortalDetail {
  const char *title;
};

inline json::Ptr to_json(const PortalDetail &detail) {
  auto obj = json::object();
  if (!obj) {
    return obj;
  }

  if (json::add(obj.get(), "title", detail.title) != ESP_OK) {
    return json::Ptr{};
  }

  return obj;
}

} // namespace earbrain::json_model
