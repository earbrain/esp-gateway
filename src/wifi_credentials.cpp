#include "earbrain/gateway/wifi_credentials.hpp"
#include "earbrain/gateway/logging.hpp"

#include <string>
#include <string_view>
#include "nvs.h"

namespace earbrain {

// NVS keys for WiFi credentials
inline constexpr const char wifi_nvs_namespace[] = "wifi";
inline constexpr const char wifi_nvs_ssid_key[] = "sta_ssid";
inline constexpr const char wifi_nvs_pass_key[] = "sta_pass";

static constexpr const char wifi_tag[] = "wifi_credentials";

esp_err_t WifiCredentialStore::save(std::string_view ssid,
                                     std::string_view passphrase) {
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(wifi_nvs_namespace, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  std::string ssid_copy(ssid);
  err = nvs_set_str(handle, wifi_nvs_ssid_key, ssid_copy.c_str());

  if (err == ESP_OK) {
    std::string pass_copy(passphrase);
    err = nvs_set_str(handle, wifi_nvs_pass_key, pass_copy.c_str());
  }

  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }

  nvs_close(handle);

  if (err == ESP_OK) {
    saved_config_.ssid.assign(ssid.data(), ssid.size());
    saved_config_.passphrase.assign(passphrase.data(), passphrase.size());
    loaded_ = true;
  }

  return err;
}

esp_err_t WifiCredentialStore::load() {
  if (loaded_) {
    return ESP_OK;
  }

  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(wifi_nvs_namespace, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    saved_config_ = StationConfig{};
    loaded_ = true;
    logging::info("No saved Wi-Fi credentials found", wifi_tag);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    return err;
  }

  size_t ssid_len = 0;
  err = nvs_get_str(handle, wifi_nvs_ssid_key, nullptr, &ssid_len);
  if (err == ESP_ERR_NVS_NOT_FOUND || ssid_len <= 1) {
    saved_config_ = StationConfig{};
    loaded_ = true;
    nvs_close(handle);
    logging::info("No saved Wi-Fi credentials found", wifi_tag);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    nvs_close(handle);
    return err;
  }

  std::string ssid_value;
  ssid_value.resize(ssid_len);
  err = nvs_get_str(handle, wifi_nvs_ssid_key, ssid_value.data(), &ssid_len);
  if (err != ESP_OK) {
    nvs_close(handle);
    return err;
  }
  if (ssid_len > 0 && ssid_value[ssid_len - 1] == '\0') {
    ssid_value.resize(ssid_len - 1);
  } else {
    ssid_value.resize(ssid_len);
  }

  size_t pass_len = 0;
  err = nvs_get_str(handle, wifi_nvs_pass_key, nullptr, &pass_len);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    return err;
  }

  std::string pass_value;
  if (err != ESP_ERR_NVS_NOT_FOUND && pass_len > 0) {
    pass_value.resize(pass_len);
    err = nvs_get_str(handle, wifi_nvs_pass_key, pass_value.data(), &pass_len);
    if (err != ESP_OK) {
      nvs_close(handle);
      return err;
    }
    if (pass_len > 0 && pass_value[pass_len - 1] == '\0') {
      pass_value.resize(pass_len - 1);
    } else {
      pass_value.resize(pass_len);
    }
  }

  nvs_close(handle);

  saved_config_.ssid = std::move(ssid_value);
  saved_config_.passphrase = std::move(pass_value);
  loaded_ = true;

  if (!saved_config_.ssid.empty()) {
    logging::infof(wifi_tag, "Loaded saved Wi-Fi credentials for SSID: %s",
                   saved_config_.ssid.c_str());
  }

  return ESP_OK;
}

std::optional<StationConfig> WifiCredentialStore::get() const {
  if (!saved_config_.ssid.empty()) {
    return saved_config_;
  }
  return std::nullopt;
}

} // namespace earbrain
