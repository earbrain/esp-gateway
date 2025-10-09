#include "earbrain/gateway/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "earbrain/gateway/handlers/device_handler.hpp"
#include "earbrain/gateway/handlers/log_handler.hpp"
#include "earbrain/gateway/handlers/metrics_handler.hpp"
#include "earbrain/gateway/handlers/mdns_handler.hpp"
#include "earbrain/gateway/handlers/portal_handler.hpp"
#include "earbrain/gateway/handlers/wifi_handler.hpp"
#include "earbrain/gateway/logging.hpp"
#include "json/http_response.hpp"
#include "json/wifi_status.hpp"
#include "json/wifi_scan.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"

namespace earbrain {

using namespace std::string_view_literals;

struct Gateway::UriHandler {
  UriHandler(std::string_view path, httpd_method_t m, RequestHandler h,
             void *ctx)
    : uri(path), method(m), handler(h), user_ctx(ctx), descriptor{} {
    refresh_descriptor();
  }

  void refresh_descriptor() {
    descriptor.uri = uri.c_str();
    descriptor.method = method;
    descriptor.handler = handler;
    descriptor.user_ctx = user_ctx;
  }

  std::string uri;
  httpd_method_t method;
  RequestHandler handler;
  void *user_ctx;
  httpd_uri_t descriptor;
};

Gateway::Gateway()
  : softap_ssid{}, softap_ssid_len(0), softap_netif(nullptr),
    sta_netif(nullptr), http_server(nullptr), server_running(false),
    wifi_initialized(false), wifi_started(false), ap_active(false),
    sta_active(false), wifi_handlers_registered(false),
    builtin_routes_registered(false), routes(), sta_connecting(false),
    sta_connected(false), sta_retry_count(0), sta_ip{},
    sta_last_disconnect_reason(WIFI_REASON_UNSPECIFIED),
    sta_last_error(ESP_OK), sta_autoconnect_attempted(false),
    wifi_credentials_store{},
    mdns_config{}, mdns_initialized(false), mdns_service_registered(false),
    mdns_running(false), mdns_registered_service_type{},
    mdns_registered_protocol{} {
  set_softap_ssid("gateway-ap"sv);
}

Gateway::~Gateway() {
  stop_server();
  if (sta_active || sta_connecting || sta_connected) {
    stop_station();
  }
  if (ap_active) {
    stop_access_point();
  }
  stop_mdns();
}

bool Gateway::has_route(std::string_view uri, httpd_method_t method) const {
  for (const auto &route : routes) {
    if (route->method == method && route->uri == uri) {
      return true;
    }
  }
  return false;
}

esp_err_t Gateway::add_route(std::string_view uri, httpd_method_t method,
                             RequestHandler handler, void *user_ctx) {
  if (uri.empty() || !handler) {
    return ESP_ERR_INVALID_ARG;
  }

  if (has_route(uri, method)) {
    return ESP_ERR_INVALID_STATE;
  }

  auto entry = std::make_unique<UriHandler>(
      uri, method, handler, user_ctx ? user_ctx : this);
  UriHandler &route = *entry;

  if (http_server) {
    const esp_err_t err = register_route_with_server(route);
    if (err != ESP_OK) {
      return err;
    }
  }

  routes.push_back(std::move(entry));
  return ESP_OK;
}

esp_err_t Gateway::register_route_with_server(UriHandler &route) const {
  if (!http_server) {
    return ESP_ERR_INVALID_STATE;
  }

  route.refresh_descriptor();
  return httpd_register_uri_handler(http_server, &route.descriptor);
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
      {"/api/v1/device", HTTP_GET, &handlers::device::handle_get},
      {"/api/v1/metrics", HTTP_GET, &handlers::metrics::handle_get},
      {"/api/v1/wifi/credentials", HTTP_POST, &handlers::wifi::handle_credentials_post},
      {"/api/v1/wifi/status", HTTP_GET, &Gateway::handle_wifi_status_get},
      {"/api/v1/wifi/scan", HTTP_GET, &Gateway::handle_wifi_scan_get},
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
  if (server_running) {
    logging::info("Server already running", "gateway");
    return ESP_OK;
  }

  ensure_builtin_routes();

  esp_err_t err = start_http_server();
  if (err != ESP_OK) {
    return err;
  }

  server_running = true;
  logging::info("HTTP server started", "gateway");

  return ESP_OK;
}

esp_err_t Gateway::stop_server() {
  if (!server_running) {
    logging::info("Server already stopped", "gateway");
    return ESP_OK;
  }

  if (http_server) {
    esp_err_t err = httpd_stop(http_server);
    if (err != ESP_OK) {
      return err;
    }
    http_server = nullptr;
  }

  server_running = false;
  logging::info("HTTP server stopped", "gateway");
  return ESP_OK;
}

esp_err_t Gateway::start_http_server() {
  if (http_server) {
    httpd_stop(http_server);
    http_server = nullptr;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  config.lru_purge_enable = true;
  // Allow slower clients more time before the server aborts the socket on send/recv.
  config.recv_wait_timeout = 20;
  config.send_wait_timeout = 30;

  esp_err_t err = httpd_start(&http_server, &config);
  if (err != ESP_OK) {
    http_server = nullptr;
    return err;
  }

  for (auto &route : routes) {
    route->user_ctx = route->user_ctx ? route->user_ctx : this;
    const esp_err_t reg_err = register_route_with_server(*route);
    if (reg_err != ESP_OK) {
      httpd_stop(http_server);
      http_server = nullptr;
      return reg_err;
    }
  }

  return ESP_OK;
}

esp_err_t Gateway::handle_wifi_status_get(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);
  if (!gateway) {
    return http::send_error(req, "Gateway unavailable", "gateway_unavailable");
  }

  WifiStatus status;
  status.ap_active = gateway->ap_active;
  status.sta_active = gateway->sta_active;
  status.sta_connecting = gateway->sta_connecting;
  status.sta_connected = gateway->sta_connected;
  status.last_error = gateway->sta_last_error;
  status.disconnect_reason = gateway->sta_last_disconnect_reason;

  const ip4_addr_t *ip4 = reinterpret_cast<const ip4_addr_t *>(&gateway->sta_ip);
  char ip_buffer[16] = {0};
  if (gateway->sta_connected && ip4addr_ntoa_r(ip4, ip_buffer, sizeof(ip_buffer))) {
    status.ip = ip_buffer;
  }

  auto data = json_model::to_json(status);
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

esp_err_t Gateway::handle_wifi_scan_get(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);
  if (!gateway) {
    return http::send_error(req, "Gateway unavailable", "gateway_unavailable");
  }

  WifiScanResult result = gateway->perform_wifi_scan();

  if (result.error != ESP_OK) {
    return http::send_error(req, "Wi-Fi scan failed", esp_err_to_name(result.error));
  }

  auto payload = json_model::to_json(result);
  if (!payload) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(payload));
}

esp_err_t Gateway::save_wifi_credentials(std::string_view ssid,
                                         std::string_view passphrase) {
  esp_err_t err = wifi_credentials_store.save(ssid, passphrase);
  if (err == ESP_OK) {
    sta_autoconnect_attempted = false;
  }
  return err;
}

void Gateway::set_sta_autoconnect_attempted(bool value) {
  sta_autoconnect_attempted = value;
}

esp_err_t Gateway::ensure_mdns_initialized() {
  if (mdns_initialized) {
    return ESP_OK;
  }

  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  err = mdns_init();
  if (err == ESP_ERR_INVALID_STATE) {
    mdns_initialized = true;
    return ESP_OK;
  }
  if (err != ESP_OK) {
    return err;
  }

  mdns_initialized = true;
  return ESP_OK;
}

esp_err_t Gateway::stop_mdns() {
  if (!mdns_initialized) {
    mdns_running = false;
    return ESP_OK;
  }

  esp_err_t first_error = ESP_OK;

  if (mdns_service_registered && !mdns_registered_service_type.empty() &&
      !mdns_registered_protocol.empty()) {
    const esp_err_t err = mdns_service_remove(
        mdns_registered_service_type.c_str(),
        mdns_registered_protocol.c_str());
    if (err != ESP_OK && first_error == ESP_OK) {
      first_error = err;
    }
  }

  mdns_service_registered = false;
  mdns_registered_service_type.clear();
  mdns_registered_protocol.clear();

  mdns_free();
  mdns_initialized = false;
  mdns_running = false;

  return first_error;
}

esp_err_t Gateway::start_mdns() { return start_mdns(mdns_config); }

esp_err_t Gateway::start_mdns(const MdnsConfig &config) {
  if (mdns_running) {
    const esp_err_t stop_err = stop_mdns();
    if (stop_err != ESP_OK) {
      return stop_err;
    }
  }

  MdnsConfig applied = config;

  esp_err_t err = ensure_mdns_initialized();
  if (err != ESP_OK) {
    return err;
  }

  err = mdns_hostname_set(applied.hostname.c_str());
  if (err != ESP_OK) {
    stop_mdns();
    return err;
  }

  err = mdns_instance_name_set(applied.instance_name.c_str());
  if (err != ESP_OK) {
    stop_mdns();
    return err;
  }

  err = mdns_service_add(nullptr, applied.service_type.c_str(),
                         applied.protocol.c_str(), applied.port, nullptr,
                         0);
  if (err != ESP_OK) {
    stop_mdns();
    return err;
  }

  mdns_config = std::move(applied);
  mdns_service_registered = true;
  mdns_registered_service_type = mdns_config.service_type;
  mdns_registered_protocol = mdns_config.protocol;
  mdns_running = true;

  logging::infof("gateway",
                 "mDNS started: host=%s instance=%s service=%s protocol=%s port=%u",
                 mdns_config.hostname.c_str(),
                 mdns_config.instance_name.c_str(),
                 mdns_config.service_type.c_str(),
                 mdns_config.protocol.c_str(),
                 static_cast<unsigned>(mdns_config.port));

  return ESP_OK;
}

void Gateway::set_softap_ssid(std::string_view ssid) {
  if (ssid.empty()) {
    return;
  }

  const std::size_t max_len = sizeof(softap_ssid) - 1;
  const std::size_t copy_len = std::min(ssid.size(), max_len);

  std::fill(std::begin(softap_ssid), std::end(softap_ssid), '\0');
  std::copy(ssid.data(), ssid.data() + copy_len, softap_ssid);
  softap_ssid_len = copy_len;
  ap_config.ssid.assign(ssid.data(), copy_len);
}

} // namespace earbrain
