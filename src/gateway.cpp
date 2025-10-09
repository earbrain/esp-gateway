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

Gateway::Gateway()
  : softap_ssid{}, softap_ssid_len(0), softap_netif(nullptr),
    sta_netif(nullptr), ap_config{}, sta_config{}, http_server{},
    builtin_routes_registered(false), wifi_initialized(false),
    wifi_started(false), ap_active(false), sta_active(false),
    wifi_handlers_registered(false), sta_connecting(false),
    sta_connected(false), sta_retry_count(0), sta_ip{},
    sta_last_disconnect_reason(WIFI_REASON_UNSPECIFIED),
    sta_last_error(ESP_OK), sta_autoconnect_attempted(false),
    wifi_credentials_store{}, mdns_service{} {
  set_softap_ssid("gateway-ap"sv);
}

Gateway::~Gateway() {
  http_server.stop();
  if (sta_active || sta_connecting || sta_connected) {
    stop_station();
  }
  if (ap_active) {
    stop_access_point();
  }
  mdns_service.stop();
}

esp_err_t Gateway::add_route(std::string_view uri, httpd_method_t method,
                             RequestHandler handler, void *user_ctx) {
  return http_server.add_route(uri, method, handler,
                               user_ctx ? user_ctx : this);
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

esp_err_t Gateway::start_mdns() {
  return mdns_service.start(mdns_service.config());
}

esp_err_t Gateway::start_mdns(const MdnsConfig &config) {
  return mdns_service.start(config);
}

esp_err_t Gateway::stop_mdns() {
  return mdns_service.stop();
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
