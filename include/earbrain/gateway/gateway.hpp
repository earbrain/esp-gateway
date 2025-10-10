#pragma once

#include "earbrain/gateway/http_server.hpp"
#include "earbrain/gateway/mdns_service.hpp"
#include "earbrain/gateway/wifi_service.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include <string_view>

namespace earbrain {

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

  esp_err_t save_wifi_credentials(std::string_view ssid, std::string_view passphrase);

  WifiService &wifi() { return wifi_service; }
  MdnsService &mdns() { return mdns_service; }
  HttpServer &server() { return http_server; }

  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, void *user_ctx = nullptr);
  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, const RouteOptions &options);

  static const char *version() { return "0.0.0"; }

private:
  void ensure_builtin_routes();

  WifiService wifi_service;
  HttpServer http_server;
  MdnsService mdns_service;
  bool builtin_routes_registered;
};

} // namespace earbrain
