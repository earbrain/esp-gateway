#pragma once

#include "earbrain/gateway/wifi_credentials.hpp"
#include "earbrain/gateway/wifi_scan.hpp"

#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_wifi_types.h"
#include <string_view>

struct esp_netif_obj;

namespace earbrain {

struct AccessPointConfig {
  std::string ssid = "gateway-ap";
  uint8_t channel = 1;
  wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
  uint8_t max_connections = 4;
};

struct WifiStatus {
  bool ap_active = false;
  bool sta_active = false;
  bool sta_connected = false;
  esp_ip4_addr_t sta_ip{};
  wifi_err_reason_t sta_last_disconnect_reason = WIFI_REASON_UNSPECIFIED;
  esp_err_t sta_last_error = ESP_OK;
};

class WifiService {
public:
  WifiService();
  ~WifiService() = default;

  esp_err_t start_access_point(const AccessPointConfig &config);
  esp_err_t start_access_point();
  esp_err_t stop_access_point();

  esp_err_t connect(const StationConfig &config);
  esp_err_t connect();

  WifiScanResult perform_scan();
  WifiStatus status() const;

  WifiCredentialStore &credentials() { return credentials_store; }

private:
  esp_err_t ensure_initialized();
  esp_err_t register_event_handlers();

  static void ip_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
  static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
  void on_sta_got_ip(const ip_event_got_ip_t &event);
  void on_sta_disconnected(const wifi_event_sta_disconnected_t &event);

  esp_netif_obj *softap_netif;
  esp_netif_obj *sta_netif;
  AccessPointConfig ap_config;
  StationConfig sta_config;
  bool initialized;
  bool handlers_registered;
  bool sta_connected;
  int sta_retry_count;
  esp_ip4_addr_t sta_ip;
  wifi_err_reason_t sta_last_disconnect_reason;
  esp_err_t sta_last_error;
  WifiCredentialStore credentials_store;
};

} // namespace earbrain
