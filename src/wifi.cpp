#include "earbrain/gateway/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"

namespace earbrain {

static constexpr const char wifi_tag[] = "gateway";

static bool is_valid_ssid(std::string_view ssid) {
  return !ssid.empty() && ssid.size() <= 32;
}

static bool is_valid_passphrase_local(std::string_view passphrase) {
  const std::size_t len = passphrase.size();
  if (len == 0) {
    return true;
  }
  if (len >= 8 && len <= 63) {
    return true;
  }
  if (len == 64) {
    return std::all_of(passphrase.begin(), passphrase.end(), [](char ch) {
      return std::isxdigit(static_cast<unsigned char>(ch));
    });
  }
  return false;
}

static wifi_config_t make_ap_config(const AccessPointConfig &config) {
  wifi_config_t cfg{};
  std::fill(std::begin(cfg.ap.ssid), std::end(cfg.ap.ssid), '\0');
  std::copy_n(config.ssid.data(), config.ssid.size(), cfg.ap.ssid);
  cfg.ap.ssid_len = config.ssid.size();
  cfg.ap.channel = config.channel;
  cfg.ap.authmode = config.auth_mode;
  cfg.ap.max_connection = config.max_connections;
  cfg.ap.pmf_cfg.capable = true;
  cfg.ap.pmf_cfg.required = false;
  return cfg;
}

static wifi_config_t make_sta_config(const StationConfig &config) {
  wifi_config_t cfg{};
  std::fill(std::begin(cfg.sta.ssid), std::end(cfg.sta.ssid), '\0');
  std::copy_n(config.ssid.data(), config.ssid.size(),
              cfg.sta.ssid);

  std::fill(std::begin(cfg.sta.password), std::end(cfg.sta.password), '\0');
  if (!config.passphrase.empty()) {
    std::copy_n(config.passphrase.data(), config.passphrase.size(),
                cfg.sta.password);
  }

  cfg.sta.pmf_cfg.capable = true;
  cfg.sta.pmf_cfg.required = false;
  cfg.sta.threshold.authmode =
      config.passphrase.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
  return cfg;
}

esp_err_t Gateway::ensure_wifi_initialized() {
  if (!nvs_initialized) {
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      esp_err_t erase_err = nvs_flash_erase();
      if (erase_err != ESP_OK) {
        return erase_err;
      }
      nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
      return nvs_err;
    }
    nvs_initialized = true;
  }

  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  if (!event_loop_created) {
    err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
      event_loop_created = true;
    } else if (err == ESP_OK) {
      event_loop_created = true;
    } else {
      return err;
    }
  }

  if (!softap_netif) {
    softap_netif = esp_netif_create_default_wifi_ap();
    if (!softap_netif) {
      return ESP_FAIL;
    }
  }

  if (!sta_netif) {
    sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
      return ESP_FAIL;
    }
  }

  if (!wifi_initialized) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
      return err;
    }
    wifi_initialized = true;
  }

  return ESP_OK;
}

esp_err_t Gateway::apply_wifi_mode() {
  if (!wifi_initialized) {
    return ESP_ERR_WIFI_NOT_INIT;
  }

  wifi_mode_t mode = WIFI_MODE_NULL;
  if (ap_active && sta_active) {
    mode = WIFI_MODE_APSTA;
  } else if (ap_active) {
    mode = WIFI_MODE_AP;
  } else if (sta_active) {
    mode = WIFI_MODE_STA;
  }

  if (mode == WIFI_MODE_NULL) {
    if (wifi_started) {
      esp_err_t err = esp_wifi_stop();
      if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT &&
          err != ESP_ERR_WIFI_NOT_STARTED) {
        return err;
      }
      wifi_started = false;
    }
    return esp_wifi_set_mode(WIFI_MODE_NULL);
  }

  esp_err_t err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) {
    return err;
  }

  if (!wifi_started) {
    err = esp_wifi_start();
    if (err != ESP_OK) {
      return err;
    }
    wifi_started = true;
  }

  return ESP_OK;
}

esp_err_t Gateway::start_access_point(const AccessPointConfig &config) {
  if (!is_valid_ssid(config.ssid)) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ensure_wifi_initialized();
  if (err != ESP_OK) {
    return err;
  }

  wifi_config_t ap_cfg = make_ap_config(config);

  const bool previous_state = ap_active;
  ap_active = true;

  err = apply_wifi_mode();
  if (err != ESP_OK) {
    ap_active = previous_state;
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
  if (err != ESP_OK) {
    ap_active = previous_state;
    apply_wifi_mode();
    return err;
  }

  ap_config = config;
  ESP_LOGI(wifi_tag, "Access point enabled: %s", ap_config.ssid.c_str());
  return ESP_OK;
}

esp_err_t Gateway::start_access_point() {
  return start_access_point(ap_config);
}

esp_err_t Gateway::stop_access_point() {
  if (!ap_active) {
    return ESP_OK;
  }

  ap_active = false;
  esp_err_t err = apply_wifi_mode();
  if (err != ESP_OK) {
    ap_active = true;
    return err;
  }

  ESP_LOGI(wifi_tag, "Access point stopped");
  return ESP_OK;
}

esp_err_t Gateway::start_station(const StationConfig &config) {
  if (!is_valid_ssid(config.ssid) ||
      !is_valid_passphrase_local(config.passphrase)) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ensure_wifi_initialized();
  if (err != ESP_OK) {
    return err;
  }

  wifi_config_t sta_cfg = make_sta_config(config);

  const bool previous_state = sta_active;
  sta_active = true;

  err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
  if (err != ESP_OK) {
    sta_active = previous_state;
    return err;
  }

  err = apply_wifi_mode();
  if (err != ESP_OK) {
    sta_active = previous_state;
    apply_wifi_mode();
    return err;
  }

  err = esp_wifi_connect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
    sta_active = previous_state;
    apply_wifi_mode();
    return err;
  }

  sta_config = config;
  ESP_LOGI(wifi_tag, "Station connection started: %s", sta_config.ssid.c_str());
  return ESP_OK;
}

esp_err_t Gateway::stop_station() {
  if (!sta_active) {
    return ESP_OK;
  }

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT &&
      err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_ERR_WIFI_NOT_CONNECT) {
    return err;
  }

  const bool previous_state = sta_active;
  sta_active = false;

  err = apply_wifi_mode();
  if (err != ESP_OK) {
    sta_active = previous_state;
    return err;
  }

  ESP_LOGI(wifi_tag, "Station stopped");
  return ESP_OK;
}

} // namespace earbrain
