#include "earbrain/gateway/wifi_credentials.hpp"
#include "earbrain/gateway/logging.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <cstring>
#include "esp_wifi.h"

namespace earbrain {

namespace {

constexpr const char wifi_tag[] = "wifi_credentials";

} // namespace

esp_err_t WifiCredentialStore::save(std::string_view ssid,
                                    std::string_view passphrase) {
  wifi_config_t wifi_config = {};

  // Copy SSID (max 32 bytes)
  size_t ssid_len = std::min(ssid.length(), sizeof(wifi_config.sta.ssid) - 1);
  std::memcpy(wifi_config.sta.ssid, ssid.data(), ssid_len);
  wifi_config.sta.ssid[ssid_len] = '\0';

  // Copy passphrase (max 64 bytes)
  size_t pass_len = std::min(passphrase.length(), sizeof(wifi_config.sta.password) - 1);
  std::memcpy(wifi_config.sta.password, passphrase.data(), pass_len);
  wifi_config.sta.password[pass_len] = '\0';

  // Set the WiFi configuration which will be stored in NVS
  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

  if (err == ESP_OK) {
    saved_config.ssid = std::string(ssid);
    saved_config.passphrase = std::string(passphrase);
    loaded = true;
    logging::infof(wifi_tag, "Saved Wi-Fi credentials for SSID: %s", saved_config.ssid.c_str());
  } else {
    logging::errorf(wifi_tag, "Failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
  }

  return err;
}

esp_err_t WifiCredentialStore::load() {
  if (loaded) {
    return ESP_OK;
  }

  wifi_config_t wifi_config = {};
  esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

  if (err != ESP_OK) {
    logging::errorf(wifi_tag, "Failed to load Wi-Fi credentials: %s", esp_err_to_name(err));
    return err;
  }

  // Check if SSID is empty
  if (wifi_config.sta.ssid[0] == '\0') {
    saved_config = StationConfig{};
    loaded = true;
    logging::info("No saved Wi-Fi credentials found", wifi_tag);
    return ESP_OK;
  }

  // Convert to C++ strings
  saved_config.ssid = std::string(reinterpret_cast<const char*>(wifi_config.sta.ssid));
  saved_config.passphrase = std::string(reinterpret_cast<const char*>(wifi_config.sta.password));
  loaded = true;

  logging::infof(wifi_tag, "Loaded saved Wi-Fi credentials for SSID: %s",
                 saved_config.ssid.c_str());

  return ESP_OK;
}

std::optional<StationConfig> WifiCredentialStore::get() {
  if (!loaded) {
    load();
  }

  if (!saved_config.ssid.empty()) {
    return saved_config;
  }
  return std::nullopt;
}

} // namespace earbrain
