#pragma once

#include <string>

namespace earbrain {

struct DeviceInfo {
  std::string model;
  std::string firmware_version;
  std::string build_time;
  std::string idf_version;
};

} // namespace earbrain
