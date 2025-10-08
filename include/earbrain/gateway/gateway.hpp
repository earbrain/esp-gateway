#pragma once

#include "earbrain/gateway/wifi_scan.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_wifi_types.h"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct esp_netif_obj;

namespace earbrain {

inline constexpr const char wifi_nvs_namespace[] = "wifi";
inline constexpr const char wifi_nvs_ssid_key[] = "sta_ssid";
inline constexpr const char wifi_nvs_pass_key[] = "sta_pass";

struct AccessPointConfig {
  std::string ssid = "gateway-ap";
  uint8_t channel = 1;
  wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
  uint8_t max_connections = 4;
};

struct StationConfig {
  std::string ssid;
  std::string passphrase;
};

struct MdnsConfig {
  std::string hostname = "esp-gateway";
  std::string instance_name = "ESP Gateway";
  std::string service_type = "_http";
  std::string protocol = "_tcp";
  uint16_t port = 80;
};

class Gateway {
public:
  Gateway();
  ~Gateway();

  esp_err_t start_access_point();
  esp_err_t start_access_point(const AccessPointConfig &config);
  esp_err_t stop_access_point();
  esp_err_t start_station();
  esp_err_t start_station(const StationConfig &config);
  esp_err_t stop_station();
  esp_err_t start_server();
  esp_err_t stop_server();
  esp_err_t start_mdns();
  esp_err_t start_mdns(const MdnsConfig &config);
  esp_err_t stop_mdns();

  void set_softap_ssid(std::string_view ssid);

  const MdnsConfig &mdns_configuration() const noexcept { return mdns_config; }
  bool mdns_is_running() const noexcept { return mdns_running; }

  using RequestHandler = esp_err_t (*)(httpd_req_t *);

  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, void *user_ctx = nullptr);

  static const char *version() { return "0.0.0"; }

private:
  struct UriHandler;

  esp_err_t start_http_server();
  void ensure_builtin_routes();
  esp_err_t register_route_with_server(UriHandler &route) const;
  bool has_route(std::string_view uri, httpd_method_t method) const;
  static esp_err_t handle_wifi_credentials_post(httpd_req_t *req);
  static esp_err_t handle_wifi_status_get(httpd_req_t *req);
  static esp_err_t handle_wifi_scan_get(httpd_req_t *req);
  static esp_err_t handle_logs_get(httpd_req_t *req);
  static void ip_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
  static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
  void on_sta_got_ip(const ip_event_got_ip_t &event);
  void on_sta_disconnected(const wifi_event_sta_disconnected_t &event);
  esp_err_t save_wifi_credentials(std::string_view ssid,
                                  std::string_view passphrase);
  esp_err_t load_wifi_credentials();
  void start_station_with_saved_profile();
  esp_err_t ensure_wifi_initialized();
  esp_err_t register_wifi_event_handlers();
  esp_err_t apply_wifi_mode();
  esp_err_t ensure_mdns_initialized();
  WifiScanResult perform_wifi_scan();

  char softap_ssid[33];
  std::size_t softap_ssid_len;
  esp_netif_obj *softap_netif;
  esp_netif_obj *sta_netif;
  AccessPointConfig ap_config;
  StationConfig sta_config;
  httpd_handle_t http_server;
  bool server_running;
  bool wifi_initialized;
  bool wifi_started;
  bool ap_active;
  bool sta_active;
  bool wifi_handlers_registered;
  bool builtin_routes_registered;
  std::vector<std::unique_ptr<UriHandler>> routes;
  bool sta_connecting;
  bool sta_connected;
  int sta_retry_count;
  esp_ip4_addr_t sta_ip;
  wifi_err_reason_t sta_last_disconnect_reason;
  esp_err_t sta_last_error;
  StationConfig saved_sta_config;
  bool has_saved_sta_credentials;
  bool sta_credentials_loaded;
  bool sta_autoconnect_attempted;
  MdnsConfig mdns_config;
  bool mdns_initialized;
  bool mdns_service_registered;
  bool mdns_running;
  std::string mdns_registered_service_type;
  std::string mdns_registered_protocol;
};

} // namespace earbrain
