#pragma once

#include "earbrain/logging.hpp"
#include "json/json_helpers.hpp"
#include "esp_log.h"

namespace earbrain::json_model {

using earbrain::logging::LogBatch;

inline const char *log_level_to_string(esp_log_level_t level) {
  switch (level) {
  case ESP_LOG_ERROR:
    return "error";
  case ESP_LOG_WARN:
    return "warn";
  case ESP_LOG_INFO:
    return "info";
  case ESP_LOG_DEBUG:
    return "debug";
  case ESP_LOG_VERBOSE:
    return "verbose";
  case ESP_LOG_NONE:
  default:
    return "none";
  }
}

inline json::Ptr to_json(const LogBatch &batch) {
  auto obj = json::object();
  if (!obj) {
    return nullptr;
  }

  json::Ptr entries{cJSON_CreateArray()};
  if (!entries) {
    return nullptr;
  }

  for (const auto &entry : batch.entries) {
    json::Ptr item = json::object();
    if (!item) {
      return nullptr;
    }

    if (!cJSON_AddNumberToObject(item.get(), "id",
                                 static_cast<double>(entry.id))) {
      return nullptr;
    }
    if (!cJSON_AddNumberToObject(item.get(), "timestamp_ms",
                                 static_cast<double>(entry.timestamp_ms))) {
      return nullptr;
    }
    if (json::add(item.get(), "level", log_level_to_string(entry.level)) !=
        ESP_OK) {
      return nullptr;
    }
    if (json::add(item.get(), "tag", entry.tag) != ESP_OK) {
      return nullptr;
    }
    if (json::add(item.get(), "message", entry.message) != ESP_OK) {
      return nullptr;
    }

    cJSON_AddItemToArray(entries.get(), item.release());
  }

  cJSON_AddItemToObject(obj.get(), "entries", entries.release());

  if (!cJSON_AddNumberToObject(obj.get(), "next_cursor",
                               static_cast<double>(batch.next_cursor))) {
    return nullptr;
  }

  if (json::add(obj.get(), "has_more", batch.has_more) != ESP_OK) {
    return nullptr;
  }

  return obj;
}

} // namespace earbrain::json_model
