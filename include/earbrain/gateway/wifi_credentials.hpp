#pragma once

#include "esp_err.h"
#include <optional>
#include <string>
#include <string_view>

namespace earbrain {

struct StationConfig {
  std::string ssid;
  std::string passphrase;
};

class WifiCredentialStore {
public:
  WifiCredentialStore() = default;

  esp_err_t save(std::string_view ssid, std::string_view passphrase);
  std::optional<StationConfig> get();

private:
  esp_err_t load();

  StationConfig saved_config_;
  bool loaded_ = false;
};

} // namespace earbrain
