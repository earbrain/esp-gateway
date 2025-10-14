#include "earbrain/gateway/gateway.hpp"

#include <cstring>
#include <string_view>

#include "earbrain/gateway/handlers/device_handler.hpp"
#include "earbrain/gateway/handlers/health_handler.hpp"
#include "earbrain/gateway/handlers/log_handler.hpp"
#include "earbrain/gateway/handlers/metrics_handler.hpp"
#include "earbrain/gateway/handlers/mdns_handler.hpp"
#include "earbrain/gateway/handlers/portal_handler.hpp"
#include "earbrain/gateway/handlers/wifi_handler.hpp"
#include "earbrain/logging.hpp"

namespace earbrain {

using namespace std::string_view_literals;

namespace {

constexpr const char gateway_tag[] = "gateway";

} // namespace

Gateway::Gateway()
  : options{},
    wifi_service{},
    http_server{},
    mdns_service{},
    builtin_routes_registered(false) {
}

Gateway::Gateway(const GatewayOptions &opts)
  : options{opts}, wifi_service{}, http_server{}, mdns_service{opts.mdns_config},
    builtin_routes_registered(false) {
}

Gateway::~Gateway() {
  http_server.stop();
  mdns_service.stop();
}

esp_err_t Gateway::add_route(std::string_view uri, httpd_method_t method,
                             RequestHandler handler, void *user_ctx) {
  return http_server.add_route(uri, method, handler,
                               user_ctx ? user_ctx : this);
}

esp_err_t Gateway::add_route(std::string_view uri, httpd_method_t method,
                             RequestHandler handler, const RouteOptions &options) {
  RouteOptions opts = options;
  if (!opts.user_ctx) {
    opts.user_ctx = this;
  }
  return http_server.add_route(uri, method, handler, opts);
}

esp_err_t Gateway::start_portal() {
  esp_err_t err = wifi_service.start_access_point(options.ap_config);
  if (err != ESP_OK) {
    logging::errorf(gateway_tag, "Failed to start access point: %s", esp_err_to_name(err));
    return err;
  }

  ensure_builtin_routes();

  err = http_server.start();
  if (err != ESP_OK) {
    logging::errorf(gateway_tag, "Failed to start HTTP server: %s", esp_err_to_name(err));
    wifi_service.stop_access_point();
    return err;
  }

  err = mdns_service.start();
  if (err != ESP_OK) {
    logging::warnf(gateway_tag, "Failed to start mDNS service: %s", esp_err_to_name(err));
  }

  logging::info("Portal started successfully", gateway_tag);
  return ESP_OK;
}

esp_err_t Gateway::stop_portal() {
  esp_err_t mdns_err = mdns_service.stop();
  if (mdns_err != ESP_OK) {
    logging::warnf(gateway_tag, "Failed to stop mDNS service: %s", esp_err_to_name(mdns_err));
  }

  esp_err_t http_err = http_server.stop();
  if (http_err != ESP_OK) {
    logging::errorf(gateway_tag, "Failed to stop HTTP server: %s", esp_err_to_name(http_err));
  }

  esp_err_t wifi_err = wifi_service.stop_access_point();
  if (wifi_err != ESP_OK) {
    logging::errorf(gateway_tag, "Failed to stop access point: %s", esp_err_to_name(wifi_err));
  }

  if (http_err != ESP_OK || wifi_err != ESP_OK) {
    return ESP_FAIL;
  }

  logging::info("Portal stopped successfully", gateway_tag);
  return ESP_OK;
}

void Gateway::on(Event event, EventListener listener) {
  event_listeners[event].push_back(std::move(listener));
}

void Gateway::emit(Event event, const StationConfig& config) {
  auto it = event_listeners.find(event);
  if (it != event_listeners.end()) {
    for (const auto& listener : it->second) {
      listener(config);
    }
  }
}

void Gateway::ensure_builtin_routes() {
  if (builtin_routes_registered) {
    return;
  }

  builtin_routes_registered = true;

  struct BuiltinRoute {
    std::string_view uri;
    httpd_method_t method;
    RequestHandler handler;
  };

  static constexpr BuiltinRoute routes_to_register[] = {
      // Portal UI routes
      {"/", HTTP_GET, &handlers::portal::handle_root_get},
      {"/wifi", HTTP_GET, &handlers::portal::handle_root_get},
      {"/device", HTTP_GET, &handlers::portal::handle_root_get},
      {"/device/info", HTTP_GET, &handlers::portal::handle_root_get},
      {"/device/metrics", HTTP_GET, &handlers::portal::handle_root_get},
      {"/device/logs", HTTP_GET, &handlers::portal::handle_root_get},
      {"/device/mdns", HTTP_GET, &handlers::portal::handle_root_get},
      // Portal assets
      {"/app.js", HTTP_GET, &handlers::portal::handle_app_js_get},
      {"/assets/index.css", HTTP_GET, &handlers::portal::handle_assets_css_get},
      // Health check
      {"/health", HTTP_GET, &handlers::health::handle_health},
      // REST API
      {"/api/v1/device", HTTP_GET, &handlers::device::handle_get},
      {"/api/v1/metrics", HTTP_GET, &handlers::metrics::handle_get},
      {"/api/v1/wifi/credentials", HTTP_POST, &handlers::wifi::handle_credentials_post},
      {"/api/v1/wifi/connect", HTTP_POST, &handlers::wifi::handle_connect_post},
      {"/api/v1/wifi/status", HTTP_GET, &handlers::wifi::handle_status_get},
      {"/api/v1/wifi/scan", HTTP_GET, &handlers::wifi::handle_scan_get},
      {"/api/v1/mdns", HTTP_GET, &handlers::mdns::handle_get},
      {"/api/v1/logs", HTTP_GET, &handlers::logs::handle_get},
  };

  for (const auto &route : routes_to_register) {
    esp_err_t err = ESP_OK;
    err = add_route(route.uri, route.method, route.handler, this);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      logging::warnf("gateway", "Failed to register builtin route %.*s: %s",
                     static_cast<int>(route.uri.size()), route.uri.data(),
                     esp_err_to_name(err));
    }
  }
}

} // namespace earbrain
