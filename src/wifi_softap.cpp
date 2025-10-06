#include "earbrain/gateway/gateway.hpp"

#include <algorithm>
#include <cstddef>

#include "esp_log.h"
#include "esp_wifi.h"

namespace earbrain {

esp_err_t Gateway::start_softap() {
  if (softap_ssid_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    return err;
  }

  wifi_config_t ap_config = {};
  std::copy_n(softap_ssid, softap_ssid_len, ap_config.ap.ssid);
  ap_config.ap.ssid_len = softap_ssid_len;
  ap_config.ap.channel = 1;
  ap_config.ap.authmode = WIFI_AUTH_OPEN;
  ap_config.ap.max_connection = 4;

  err = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (err == ESP_OK) {
    err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  }

  if (err == ESP_OK) {
    err = esp_wifi_start();
  }

  if (err != ESP_OK) {
    esp_wifi_deinit();
    return err;
  }
  ESP_LOGI("gateway", "SoftAP started in AP+STA mode: %.*s", ap_config.ap.ssid_len,
           ap_config.ap.ssid);
  return ESP_OK;
}

} // namespace earbrain
