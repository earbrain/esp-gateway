#include "earbrain/gateway/device_info.hpp"

#include <cJSON.h>

#include <optional>
#include <string>

namespace earbrain {

std::string DeviceInfo::to_json() const {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return {};
  }

  cJSON_AddStringToObject(root, "model", model.c_str());
  cJSON_AddStringToObject(root, "firmware_version", firmware_version.c_str());
  cJSON_AddStringToObject(root, "build_time", build_time.c_str());
  cJSON_AddStringToObject(root, "idf_version", idf_version.c_str());

  char *json_buffer = cJSON_PrintUnformatted(root);
  std::string json_text =
      json_buffer ? std::string(json_buffer) : std::string{};
  if (json_buffer) {
    cJSON_free(json_buffer);
  }
  cJSON_Delete(root);
  return json_text;
}

std::optional<DeviceInfo> DeviceInfo::from_json(std::string_view json) {
  std::string json_copy(json);
  cJSON *root = cJSON_Parse(json_copy.c_str());
  if (!root) {
    return std::nullopt;
  }

  DeviceInfo device_info{};
  auto fetch_string = [&](const char *key, std::string &target) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) {
      return false;
    }
    target.assign(item->valuestring);
    return true;
  };

  const bool is_ok =
      fetch_string("model", device_info.model) &&
      fetch_string("firmware_version", device_info.firmware_version) &&
      fetch_string("build_time", device_info.build_time) &&
      fetch_string("idf_version", device_info.idf_version);
  cJSON_Delete(root);

  if (!is_ok) {
    return std::nullopt;
  }

  return device_info;
}

} // namespace earbrain
