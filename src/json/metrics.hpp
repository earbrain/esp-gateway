#pragma once

#include "earbrain/gateway/metrics.hpp"
#include "json/json_helpers.hpp"

#include <cJSON.h>

namespace earbrain::json_model {

inline json::Ptr to_json(const Metrics &metrics) {
  auto obj = json::object();
  if (!obj) {
    return nullptr;
  }

  if (!cJSON_AddNumberToObject(obj.get(), "heap_total",
                               static_cast<double>(metrics.heap_total))) {
    return nullptr;
  }
  if (!cJSON_AddNumberToObject(obj.get(), "heap_free",
                               static_cast<double>(metrics.heap_free))) {
    return nullptr;
  }
  if (!cJSON_AddNumberToObject(obj.get(), "heap_used",
                               static_cast<double>(metrics.heap_used))) {
    return nullptr;
  }
  if (!cJSON_AddNumberToObject(obj.get(), "heap_min_free",
                               static_cast<double>(metrics.heap_min_free))) {
    return nullptr;
  }
  if (!cJSON_AddNumberToObject(obj.get(), "heap_largest_free_block",
                               static_cast<double>(
                                 metrics.heap_largest_free_block))) {
    return nullptr;
  }
  if (!cJSON_AddNumberToObject(obj.get(), "timestamp_ms",
                               static_cast<double>(metrics.timestamp_ms))) {
    return nullptr;
  }

  return obj;
}

} // namespace earbrain::json_model
