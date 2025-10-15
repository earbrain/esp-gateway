#pragma once

#include "earbrain/gateway/http_server.hpp"
#include "earbrain/mdns_service.hpp"
#include "earbrain/wifi_service.hpp"

#include "esp_err.h"
#include "esp_http_server.h"
#include <functional>
#include <map>
#include <string_view>
#include <vector>

namespace earbrain {

struct GatewayOptions {
  AccessPointConfig ap_config{"gateway-ap"};
  MdnsConfig mdns_config{"esp-gateway", "ESP Gateway", "_http", "_tcp", 80};
};

class Gateway {
public:
  enum class Event {
    WifiCredentialsSaved,
    WifiConnectSuccess,
    WifiConnectFailed,
  };

  using EventListener = std::function<void(const WifiCredentials&)>;

  Gateway();
  Gateway(const GatewayOptions &options);
  ~Gateway();

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

  void on(Event event, EventListener listener);
  void emit(Event event, const WifiCredentials& credentials);

  static const char *version() {
#ifdef GATEWAY_VERSION
    return GATEWAY_VERSION;
#else
    return "unknown";
#endif
  }

private:
  void ensure_builtin_routes();

  GatewayOptions options;
  HttpServer http_server;
  bool builtin_routes_registered;
  std::map<Event, std::vector<EventListener>> event_listeners;
};

} // namespace earbrain
