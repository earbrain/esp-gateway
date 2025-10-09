#include "earbrain/gateway/wifi_service.hpp"
#include "earbrain/gateway/logging.hpp"
#include "earbrain/gateway/validation.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/ip4_addr.h"

namespace earbrain {

namespace {

constexpr const char wifi_tag[] = "wifi";

constexpr uint8_t sta_listen_interval = 1;
constexpr int8_t sta_tx_power_qdbm = 78;
constexpr int sta_max_connect_retries = 5;

int signal_quality_from_rssi(int32_t rssi) {
  if (rssi <= -100) {
    return 0;
  }
  if (rssi >= -50) {
    return 100;
  }
  const int quality = 2 * (static_cast<int>(rssi) + 100);
  return std::clamp(quality, 0, 100);
}

std::string format_bssid(const uint8_t (&bssid)[6]) {
  char buffer[18] = {0};
  std::snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  return std::string(buffer);
}

wifi_config_t make_ap_config(const AccessPointConfig &config) {
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

wifi_config_t make_sta_config(const StationConfig &config) {
  wifi_config_t cfg{};
  std::fill(std::begin(cfg.sta.ssid), std::end(cfg.sta.ssid), '\0');
  std::copy_n(config.ssid.data(), config.ssid.size(), cfg.sta.ssid);

  std::fill(std::begin(cfg.sta.password), std::end(cfg.sta.password), '\0');
  if (!config.passphrase.empty()) {
    std::copy_n(config.passphrase.data(), config.passphrase.size(),
                cfg.sta.password);
  }

  cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  cfg.sta.listen_interval = sta_listen_interval;
  cfg.sta.pmf_cfg.capable = true;
  cfg.sta.pmf_cfg.required = false;
  cfg.sta.threshold.authmode =
      config.passphrase.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
  return cfg;
}

bool should_retry_reason(wifi_err_reason_t reason) {
  switch (reason) {
  case WIFI_REASON_AUTH_LEAVE:
  case WIFI_REASON_ASSOC_LEAVE:
  case WIFI_REASON_STA_LEAVING:
    return false;
  default:
    return true;
  }
}

} // namespace

WifiService::WifiService()
  : softap_netif(nullptr), sta_netif(nullptr), ap_config{}, sta_config{},
    initialized(false), started(false), ap_active(false), sta_active(false),
    handlers_registered(false), sta_connecting(false), sta_connected(false),
    sta_retry_count(0), sta_ip{},
    sta_last_disconnect_reason(WIFI_REASON_UNSPECIFIED),
    sta_last_error(ESP_OK), autoconnect_attempted(false), credentials_store{} {
}

esp_err_t WifiService::ensure_initialized() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      return err;
    }
    err = nvs_flash_init();
  }
  if (err != ESP_OK && err != ESP_ERR_NVS_INVALID_STATE) {
    return err;
  }

  err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
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

  if (!initialized) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
      return err;
    }
    initialized = true;
  }

  if (!credentials_store.is_loaded()) {
    err = credentials_store.load();
    if (err != ESP_OK) {
      return err;
    }
  }

  err = register_event_handlers();
  if (err != ESP_OK) {
    return err;
  }

  return ESP_OK;
}

esp_err_t WifiService::register_event_handlers() {
  if (handlers_registered) {
    return ESP_OK;
  }

  esp_err_t err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &WifiService::ip_event_handler, this);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                   &WifiService::wifi_event_handler, this);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                 &WifiService::ip_event_handler);
    return err;
  }

  handlers_registered = true;
  return ESP_OK;
}

esp_err_t WifiService::apply_mode() {
  if (!initialized) {
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
    if (started) {
      esp_err_t err = esp_wifi_stop();
      if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT &&
          err != ESP_ERR_WIFI_NOT_STARTED) {
        return err;
      }
      started = false;
    }
    return esp_wifi_set_mode(WIFI_MODE_NULL);
  }

  esp_err_t err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) {
    return err;
  }

  if (!started) {
    err = esp_wifi_start();
    if (err != ESP_OK) {
      return err;
    }
    started = true;
  }

  return ESP_OK;
}

esp_err_t WifiService::start_access_point(const AccessPointConfig &config) {
  if (!validation::is_valid_ssid(config.ssid)) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ensure_initialized();
  if (err != ESP_OK) {
    return err;
  }

  wifi_config_t ap_cfg = make_ap_config(config);

  const bool previous_state = ap_active;
  ap_active = true;

  err = apply_mode();
  if (err != ESP_OK) {
    ap_active = previous_state;
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
  if (err != ESP_OK) {
    ap_active = previous_state;
    apply_mode();
    return err;
  }

  ap_config = config;
  logging::infof(wifi_tag, "Access point enabled: %s", ap_config.ssid.c_str());
  start_station_with_saved_profile();
  return ESP_OK;
}

esp_err_t WifiService::start_access_point() {
  return start_access_point(ap_config);
}

esp_err_t WifiService::stop_access_point() {
  if (!ap_active) {
    return ESP_OK;
  }

  ap_active = false;
  esp_err_t err = apply_mode();
  if (err != ESP_OK) {
    ap_active = true;
    return err;
  }

  logging::info("Access point stopped", wifi_tag);
  return ESP_OK;
}

esp_err_t WifiService::start_station(const StationConfig &config) {
  if (!validation::is_valid_ssid(config.ssid) ||
      !validation::is_valid_passphrase(config.passphrase)) {
    sta_last_error = ESP_ERR_INVALID_ARG;
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ensure_initialized();
  if (err != ESP_OK) {
    sta_last_error = err;
    return err;
  }

  const bool previous_state = sta_active;
  sta_active = true;

  err = apply_mode();
  if (err != ESP_OK) {
    sta_last_error = err;
    sta_active = previous_state;
    apply_mode();
    return err;
  }

  wifi_config_t sta_cfg = make_sta_config(config);

  err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
  if (err != ESP_OK) {
    sta_last_error = err;
    sta_active = previous_state;
    apply_mode();
    return err;
  }

  esp_err_t ps_err = esp_wifi_set_ps(WIFI_PS_NONE);
  if (ps_err != ESP_OK) {
    logging::warnf(wifi_tag, "Failed to disable power save: %s",
                   esp_err_to_name(ps_err));
  }

  ps_err = esp_wifi_set_max_tx_power(sta_tx_power_qdbm);
  if (ps_err != ESP_OK) {
    logging::warnf(wifi_tag, "Failed to raise STA TX power: %s",
                   esp_err_to_name(ps_err));
  }

  err = esp_wifi_connect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
    sta_last_error = err;
    sta_active = previous_state;
    apply_mode();
    return err;
  }

  sta_config = config;
  sta_connecting = true;
  sta_connected = false;
  sta_retry_count = 0;
  sta_last_error = ESP_OK;
  sta_last_disconnect_reason = WIFI_REASON_UNSPECIFIED;
  sta_ip.addr = 0;
  logging::infof(wifi_tag, "Station connection started: ssid='%s' (len=%zu)",
                 sta_config.ssid.c_str(), sta_config.ssid.size());
  return ESP_OK;
}

esp_err_t WifiService::start_station() {
  const esp_err_t init_err = ensure_initialized();
  if (init_err != ESP_OK) {
    return init_err;
  }

  auto credentials = credentials_store.get();
  if (!credentials) {
    return ESP_ERR_NOT_FOUND;
  }

  logging::infof(wifi_tag,
                 "Attempting auto-connect to saved SSID: '%s' (len=%zu)",
                 credentials->ssid.c_str(), credentials->ssid.size());

  autoconnect_attempted = true;

  return start_station(*credentials);
}

esp_err_t WifiService::stop_station() {
  if (!sta_active) {
    return ESP_OK;
  }

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT &&
      err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_ERR_WIFI_NOT_CONNECT) {
    sta_last_error = err;
    return err;
  }

  const bool previous_state = sta_active;
  sta_active = false;

  err = apply_mode();
  if (err != ESP_OK) {
    sta_active = previous_state;
    sta_last_error = err;
    return err;
  }

  sta_connecting = false;
  sta_connected = false;
  sta_retry_count = 0;
  sta_last_error = ESP_OK;
  sta_last_disconnect_reason = WIFI_REASON_UNSPECIFIED;
  sta_ip.addr = 0;
  logging::info("Station stopped", wifi_tag);
  return ESP_OK;
}

void WifiService::ip_event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
  if (event_base != IP_EVENT || event_id != IP_EVENT_STA_GOT_IP || !event_data) {
    return;
  }
  auto *wifi = static_cast<WifiService *>(arg);
  if (!wifi) {
    return;
  }
  const auto *event = static_cast<ip_event_got_ip_t *>(event_data);
  wifi->on_sta_got_ip(*event);
}

void WifiService::wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  if (event_base != WIFI_EVENT) {
    return;
  }
  auto *wifi = static_cast<WifiService *>(arg);
  if (!wifi) {
    return;
  }

  switch (event_id) {
  case WIFI_EVENT_STA_DISCONNECTED:
    if (event_data) {
      const auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
      wifi->on_sta_disconnected(*event);
    }
    break;
  default:
    break;
  }
}

void WifiService::on_sta_got_ip(const ip_event_got_ip_t &event) {
  sta_connecting = false;
  sta_connected = true;
  sta_retry_count = 0;
  sta_last_error = ESP_OK;
  sta_ip = event.ip_info.ip;
  sta_last_disconnect_reason = WIFI_REASON_UNSPECIFIED;

  const ip4_addr_t *ip4 = reinterpret_cast<const ip4_addr_t *>(&sta_ip);
  char ip_buffer[16] = {0};
  ip4addr_ntoa_r(ip4, ip_buffer, sizeof(ip_buffer));
  logging::infof(wifi_tag, "Station got IP: %s", ip_buffer);
}

void WifiService::on_sta_disconnected(const wifi_event_sta_disconnected_t &event) {
  sta_connecting = false;
  sta_connected = false;
  sta_last_disconnect_reason = static_cast<wifi_err_reason_t>(event.reason);
  sta_ip.addr = 0;
  logging::warnf(wifi_tag, "Station disconnected (reason=%d)",
                 static_cast<int>(event.reason));

  const wifi_err_reason_t reason = static_cast<wifi_err_reason_t>(event.reason);
  if (sta_active && should_retry_reason(reason) &&
      sta_retry_count < sta_max_connect_retries) {
    ++sta_retry_count;
    const esp_err_t err = esp_wifi_connect();
    if (err == ESP_OK || err == ESP_ERR_WIFI_CONN) {
      logging::infof(wifi_tag, "Retrying station connection (attempt %d/%d)",
                     sta_retry_count, sta_max_connect_retries);
    } else {
      logging::warnf(wifi_tag, "Failed to trigger reconnect attempt %d: %s",
                     sta_retry_count, esp_err_to_name(err));
    }
  } else if (sta_retry_count >= sta_max_connect_retries) {
    logging::warnf(wifi_tag, "Station retries exhausted after %d attempts",
                   sta_max_connect_retries);
  }
}

WifiScanResult WifiService::perform_scan() {
  WifiScanResult result{};

  esp_err_t err = ensure_initialized();
  if (err != ESP_OK) {
    result.error = err;
    return result;
  }

  const bool previous_sta_active = sta_active;
  if (!sta_active) {
    sta_active = true;
  }

  err = apply_mode();
  if (err != ESP_OK) {
    if (!previous_sta_active) {
      sta_active = false;
      esp_err_t mode_err = apply_mode();
      if (mode_err != ESP_OK) {
        logging::warnf(wifi_tag,
                       "Failed to restore AP-only mode after scan setup: %s",
                       esp_err_to_name(mode_err));
      }
    }
    result.error = err;
    return result;
  }

  wifi_scan_config_t scan_cfg{};
  scan_cfg.show_hidden = true;

  err = esp_wifi_scan_start(&scan_cfg, true);
  if (err != ESP_OK) {
    if (!previous_sta_active) {
      sta_active = false;
      esp_err_t mode_err = apply_mode();
      if (mode_err != ESP_OK) {
        logging::warnf(wifi_tag,
                       "Failed to restore AP-only mode after scan failure: %s",
                       esp_err_to_name(mode_err));
      }
    }
    result.error = err;
    return result;
  }

  uint16_t ap_count = 0;
  err = esp_wifi_scan_get_ap_num(&ap_count);
  std::vector<wifi_ap_record_t> records;
  if (err == ESP_OK && ap_count > 0) {
    records.resize(ap_count);
    err = esp_wifi_scan_get_ap_records(&ap_count, records.data());
    if (err == ESP_OK) {
      records.resize(ap_count);
    } else {
      records.clear();
    }
  }

  if (!previous_sta_active) {
    sta_active = false;
    esp_err_t mode_err = apply_mode();
    if (mode_err != ESP_OK) {
      logging::warnf(wifi_tag,
                     "Failed to restore AP-only mode after scan: %s",
                     esp_err_to_name(mode_err));
    }
  }

  if (err != ESP_OK) {
    result.error = err;
    return result;
  }

  result.networks.reserve(records.size());
  const bool is_connected = sta_connected;
  const std::string current_ssid =
      is_connected ? sta_config.ssid : std::string{};

  for (const auto &record : records) {
    const char *ssid_raw = reinterpret_cast<const char *>(record.ssid);
    if (!ssid_raw || ssid_raw[0] == '\0') {
      continue;
    }

    WifiNetworkSummary summary{};
    summary.ssid = ssid_raw;
    summary.bssid = format_bssid(record.bssid);
    summary.rssi = record.rssi;
    summary.signal = signal_quality_from_rssi(record.rssi);
    summary.channel = record.primary;
    summary.auth_mode = record.authmode;
    summary.hidden = record.ssid[0] == '\0';
    summary.connected = is_connected && !current_ssid.empty() &&
                        current_ssid == summary.ssid;
    result.networks.push_back(std::move(summary));
  }

  std::sort(result.networks.begin(), result.networks.end(),
            [](const WifiNetworkSummary &a, const WifiNetworkSummary &b) {
              return a.signal > b.signal;
            });

  result.error = ESP_OK;
  return result;
}

WifiStatus WifiService::status() const {
  WifiStatus s;
  s.ap_active = ap_active;
  s.sta_active = sta_active;
  s.sta_connecting = sta_connecting;
  s.sta_connected = sta_connected;
  s.sta_ip = sta_ip;
  s.sta_last_disconnect_reason = sta_last_disconnect_reason;
  s.sta_last_error = sta_last_error;
  return s;
}

void WifiService::start_station_with_saved_profile() {
  if (autoconnect_attempted) {
    return;
  }

  const esp_err_t err = start_station();
  if (err == ESP_ERR_NOT_FOUND) {
    return;
  }
  if (err != ESP_OK) {
    logging::warnf(wifi_tag, "Auto station connect failed: %s",
                   esp_err_to_name(err));
  }
}

} // namespace earbrain
