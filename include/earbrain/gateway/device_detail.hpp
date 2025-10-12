#pragma once

#include <string>

namespace earbrain {

struct DeviceDetail {
  std::string model;
  std::string gateway_version;
  std::string build_time;
  std::string idf_version;
};

} // namespace earbrain
