#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace earbrain {

struct DeviceInfo {
  std::string model;
  std::string firmware_version;
  std::string build_time;
  std::string idf_version;

  std::string to_json() const;

  static std::optional<DeviceInfo> from_json(std::string_view json);
};

} // namespace earbrain
