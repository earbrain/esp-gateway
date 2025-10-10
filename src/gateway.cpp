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
#include "earbrain/gateway/logging.hpp"
#include "lwip/ip4_addr.h"

namespace earbrain {

using namespace std::string_view_literals;

Gateway::Gateway()
  : wifi_service{}, http_server{}, mdns_service{},
    builtin_routes_registered(false) {}

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
      {"/", HTTP_GET, &handlers::portal::handle_root_get},
      {"/wifi", HTTP_GET, &handlers::portal::handle_root_get},
      {"/device", HTTP_GET, &handlers::portal::handle_root_get},
      {"/app.js", HTTP_GET, &handlers::portal::handle_app_js_get},
      {"/assets/index.css", HTTP_GET, &handlers::portal::handle_assets_css_get},
      {"/health", HTTP_GET, &handlers::health::handle_health},
      {"/api/v1/device", HTTP_GET, &handlers::device::handle_get},
      {"/api/v1/metrics", HTTP_GET, &handlers::metrics::handle_get},
      {"/api/v1/wifi/credentials", HTTP_POST, &handlers::wifi::handle_credentials_post},
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

esp_err_t Gateway::start_server() {
  if (http_server.is_running()) {
    logging::info("Server already running", "gateway");
    return ESP_OK;
  }

  ensure_builtin_routes();

  esp_err_t err = http_server.start();
  if (err != ESP_OK) {
    return err;
  }

  logging::info("HTTP server started", "gateway");
  return ESP_OK;
}

esp_err_t Gateway::stop_server() {
  if (!http_server.is_running()) {
    logging::info("Server already stopped", "gateway");
    return ESP_OK;
  }

  esp_err_t err = http_server.stop();
  if (err != ESP_OK) {
    return err;
  }

  logging::info("HTTP server stopped", "gateway");
  return ESP_OK;
}

esp_err_t Gateway::start_access_point() {
  return wifi_service.start_access_point();
}

esp_err_t Gateway::start_access_point(const AccessPointConfig &config) {
  return wifi_service.start_access_point(config);
}

esp_err_t Gateway::stop_access_point() {
  return wifi_service.stop_access_point();
}

esp_err_t Gateway::start_station() {
  return wifi_service.start_station();
}

esp_err_t Gateway::start_station(const StationConfig &config) {
  return wifi_service.start_station(config);
}

esp_err_t Gateway::stop_station() {
  return wifi_service.stop_station();
}

esp_err_t Gateway::save_wifi_credentials(std::string_view ssid,
                                         std::string_view passphrase) {
  esp_err_t err = wifi_service.credentials().save(ssid, passphrase);
  if (err == ESP_OK) {
    wifi_service.set_autoconnect_attempted(false);
  }
  return err;
}

esp_err_t Gateway::start_mdns() {
  return mdns_service.start(mdns_service.config());
}

esp_err_t Gateway::start_mdns(const MdnsConfig &config) {
  return mdns_service.start(config);
}

esp_err_t Gateway::stop_mdns() {
  return mdns_service.stop();
}

} // namespace earbrain
