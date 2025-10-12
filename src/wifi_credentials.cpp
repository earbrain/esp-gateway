#include "earbrain/gateway/wifi_credentials.hpp"
#include "earbrain/gateway/logging.hpp"

#include <optional>
#include <string>
#include <string_view>
#include "nvs.h"

namespace earbrain {

namespace {

// NVS keys for WiFi credentials
constexpr const char wifi_nvs_namespace[] = "wifi";
constexpr const char wifi_nvs_ssid_key[] = "sta_ssid";
constexpr const char wifi_nvs_pass_key[] = "sta_pass";

constexpr const char wifi_tag[] = "wifi_credentials";

std::optional<std::string> read_nvs_string(nvs_handle_t handle, const char *key, esp_err_t &err) {
  size_t len = 0;
  err = nvs_get_str(handle, key, nullptr, &len);

  if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = ESP_OK;
    return std::nullopt;
  }

  if (err != ESP_OK) {
    return std::nullopt;
  }

  if (len <= 1) {
    err = ESP_OK;
    return std::nullopt;
  }

  std::string value;
  value.resize(len);
  err = nvs_get_str(handle, key, value.data(), &len);

  if (err != ESP_OK) {
    return std::nullopt;
  }

  // Remove trailing null terminator if present
  if (len > 0 && value[len - 1] == '\0') {
    value.resize(len - 1);
  } else {
    value.resize(len);
  }

  return value;
}

} // namespace

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
    saved_config.ssid = std::string(ssid);
    saved_config.passphrase = std::string(passphrase);
    loaded = true;
  }

  return err;
}

esp_err_t WifiCredentialStore::load() {
  if (loaded) {
    return ESP_OK;
  }

  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(wifi_nvs_namespace, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    saved_config = StationConfig{};
    loaded = true;
    logging::info("No saved Wi-Fi credentials found", wifi_tag);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    return err;
  }

  // Read SSID
  auto ssid_opt = read_nvs_string(handle, wifi_nvs_ssid_key, err);
  if (err != ESP_OK) {
    nvs_close(handle);
    return err;
  }

  if (!ssid_opt) {
    saved_config = StationConfig{};
    loaded = true;
    nvs_close(handle);
    logging::info("No saved Wi-Fi credentials found", wifi_tag);
    return ESP_OK;
  }

  // Read passphrase
  auto pass_opt = read_nvs_string(handle, wifi_nvs_pass_key, err);
  if (err != ESP_OK) {
    nvs_close(handle);
    return err;
  }

  nvs_close(handle);

  saved_config.ssid = std::move(*ssid_opt);
  saved_config.passphrase = pass_opt ? std::move(*pass_opt) : std::string{};
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
