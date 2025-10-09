#pragma once

#include "esp_err.h"
#include <string>

namespace earbrain {

struct MdnsConfig {
  std::string hostname = "esp-gateway";
  std::string instance_name = "ESP Gateway";
  std::string service_type = "_http";
  std::string protocol = "_tcp";
  uint16_t port = 80;
};

class MdnsService {
public:
  MdnsService() = default;

  esp_err_t start(const MdnsConfig &config);
  esp_err_t stop();

  bool is_running() const noexcept { return running; }
  const MdnsConfig &config() const noexcept { return config_; }

private:
  esp_err_t ensure_initialized();

  MdnsConfig config_;
  bool initialized = false;
  bool service_registered = false;
  bool running = false;
  std::string registered_service_type;
  std::string registered_protocol;
};

} // namespace earbrain
