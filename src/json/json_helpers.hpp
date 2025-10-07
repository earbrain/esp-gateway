#pragma once

#include "esp_err.h"
#include <cJSON.h>

#include <memory>
#include <string>
#include <string_view>

namespace earbrain::json {

struct Deleter {
  void operator()(cJSON *ptr) const noexcept {
    if (ptr) {
      cJSON_Delete(ptr);
    }
  }
};

using Ptr = std::unique_ptr<cJSON, Deleter>;

inline Ptr object() { return Ptr{cJSON_CreateObject()}; }

inline Ptr parse(std::string_view text) {
  return Ptr{cJSON_ParseWithLength(text.data(), text.size())};
}

inline esp_err_t add(cJSON *obj, const char *key, std::string_view value) {
  if (!obj || !key) {
    return ESP_ERR_INVALID_ARG;
  }
  std::string copy{value};
  return cJSON_AddStringToObject(obj, key, copy.c_str())
           ? ESP_OK
           : ESP_ERR_NO_MEM;
}

inline esp_err_t add(cJSON *obj, const char *key, const char *value) {
  return add(obj, key, value ? std::string_view{value} : std::string_view{});
}

inline esp_err_t add(cJSON *obj, const char *key, bool value) {
  if (!obj || !key) {
    return ESP_ERR_INVALID_ARG;
  }
  return cJSON_AddBoolToObject(obj, key, value) ? ESP_OK : ESP_ERR_NO_MEM;
}

inline bool required_string(cJSON *root, const char *key, std::string &out) {
  if (!root || !key) {
    return false;
  }
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsString(item) || item->valuestring == nullptr) {
    return false;
  }
  out = item->valuestring;
  return true;
}

} // namespace earbrain::json
