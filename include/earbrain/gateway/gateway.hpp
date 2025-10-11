#pragma once

#include "earbrain/gateway/http_server.hpp"
#include "earbrain/gateway/mdns_service.hpp"
#include "earbrain/gateway/wifi_service.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include <string_view>

namespace earbrain {

struct GatewayOptions {
  AccessPointConfig ap_config{"gateway-ap"};
  MdnsConfig mdns_config{"esp-gateway", "ESP Gateway", "_http", "_tcp", 80};
};

class Gateway {
public:
  Gateway();
  Gateway(const GatewayOptions &options);
  ~Gateway();

  WifiService &wifi() { return wifi_service; }
  MdnsService &mdns() { return mdns_service; }
  HttpServer &server() {
    ensure_builtin_routes();
    return http_server;
  }

  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, void *user_ctx = nullptr);
  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, const RouteOptions &options);

  static const char *version() { return "0.0.0"; }

private:
  void ensure_builtin_routes();

  GatewayOptions options;
  WifiService wifi_service;
  HttpServer http_server;
  MdnsService mdns_service;
  bool builtin_routes_registered;
};

} // namespace earbrain
