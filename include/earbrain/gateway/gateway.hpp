#pragma once

#include "earbrain/gateway/http_server.hpp"
#include "earbrain/mdns_service.hpp"
#include "earbrain/wifi_service.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include <string_view>

namespace earbrain {

struct PortalConfig {
  const char* title = "ESP Gateway Portal";
};

struct GatewayOptions {
  AccessPointConfig ap_config{"gateway-ap"};
  MdnsConfig mdns_config{"esp-gateway", "ESP Gateway", "_http", "_tcp", 80};
  PortalConfig portal_config{};
};

class Gateway {
public:
  Gateway();
  ~Gateway();

  // Delete copy and move constructors and assignment operators
  Gateway(const Gateway &) = delete;
  Gateway &operator=(const Gateway &) = delete;
  Gateway(Gateway &&) = delete;
  Gateway &operator=(Gateway &&) = delete;

  esp_err_t initialize(const GatewayOptions &opts);

  HttpServer &server() {
    ensure_builtin_routes();
    return http_server;
  }

  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, void *user_ctx = nullptr);
  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, const RouteOptions &options);

  esp_err_t start_portal();
  esp_err_t stop_portal();

  static const char *version() {
#ifdef GATEWAY_VERSION
    return GATEWAY_VERSION;
#else
    return "unknown";
#endif
  }

  GatewayOptions options;

private:
  void ensure_builtin_routes();
  HttpServer http_server;
  bool builtin_routes_registered;
};

Gateway &gateway();

} // namespace earbrain
