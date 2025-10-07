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

#include "earbrain/gateway/device_info.hpp"
#include "json/device_info.hpp"
#include "json/http_response.hpp"
#include "json/json_helpers.hpp"
#include "json/wifi_credentials.hpp"
#include "json/wifi_status.hpp"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/ip4_addr.h"

extern "C" {
extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];
extern const uint8_t _binary_app_js_start[];
extern const uint8_t _binary_app_js_end[];
extern const uint8_t _binary_index_css_start[];
extern const uint8_t _binary_index_css_end[];
}

namespace earbrain {

using namespace std::string_view_literals;

static constexpr auto html_content_type = "text/html; charset=utf-8";
static constexpr auto js_content_type = "application/javascript";
static constexpr auto css_content_type = "text/css";
static constexpr size_t max_request_body_size = 1024;

static bool is_valid_passphrase(std::string_view passphrase) {
  const std::size_t len = passphrase.size();
  if (len >= 8 && len <= 63) {
    return true;
  }

  if (len == 64) {
    return std::all_of(passphrase.begin(), passphrase.end(), [](char ch) {
      return std::isxdigit(static_cast<unsigned char>(ch));
    });
  }

  return false;
}

static const char *chip_model_string(const esp_chip_info_t &info) {
  switch (info.model) {
  case CHIP_ESP32:
    return "ESP32";
  case CHIP_ESP32S2:
    return "ESP32-S2";
  case CHIP_ESP32S3:
    return "ESP32-S3";
  case CHIP_ESP32C3:
    return "ESP32-C3";
  case CHIP_ESP32C2:
    return "ESP32-C2";
  case CHIP_ESP32C6:
    return "ESP32-C6";
  case CHIP_ESP32H2:
    return "ESP32-H2";
  default:
    return "Unknown";
  }
}

static constexpr const char build_timestamp[] = __DATE__ " " __TIME__;

constexpr const uint8_t *truncate_null_terminator(const uint8_t *begin,
                                                  const uint8_t *end) {
  return (begin != end && *(end - 1) == '\0') ? end - 1 : end;
}

static esp_err_t send_embedded(httpd_req_t *req, const uint8_t *begin,
                               const uint8_t *end) {
  end = truncate_null_terminator(begin, end);
  static constexpr size_t chunk_size = 1024;
  size_t remaining = static_cast<size_t>(end - begin);
  const uint8_t *cursor = begin;

  while (remaining > 0) {
    const size_t to_send = remaining > chunk_size ? chunk_size : remaining;
    const esp_err_t err = httpd_resp_send_chunk(
        req, reinterpret_cast<const char *>(cursor), to_send);
    if (err != ESP_OK) {
      return err;
    }
    cursor += to_send;
    remaining -= to_send;
  }

  return httpd_resp_send_chunk(req, nullptr, 0);
}

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
    sta_connected(false), sta_ip{},
    sta_last_disconnect_reason(WIFI_REASON_UNSPECIFIED),
    sta_last_error(ESP_OK), saved_sta_config{}, has_saved_sta_credentials(false),
    sta_credentials_loaded(false), sta_autoconnect_attempted(false),
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
      {"/", HTTP_GET, &Gateway::handle_root_get},
      {"/wifi", HTTP_GET, &Gateway::handle_root_get},
      {"/device", HTTP_GET, &Gateway::handle_root_get},
      {"/app.js", HTTP_GET, &Gateway::handle_app_js_get},
      {"/assets/index.css", HTTP_GET, &Gateway::handle_assets_css_get},
      {"/api/v1/device-info", HTTP_GET, &Gateway::handle_device_info_get},
      {"/api/v1/wifi/credentials", HTTP_POST,
       &Gateway::handle_wifi_credentials_post},
      {"/api/v1/wifi/status", HTTP_GET, &Gateway::handle_wifi_status_get},
  };

  for (const auto &route : routes_to_register) {
    esp_err_t err = ESP_OK;
    err = add_route(route.uri, route.method, route.handler, this);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW("gateway", "Failed to register builtin route %.*s: %s",
               static_cast<int>(route.uri.size()), route.uri.data(),
               esp_err_to_name(err));
    }
  }
}

esp_err_t Gateway::start_server() {
  if (server_running) {
    ESP_LOGI("gateway", "Server already running");
    return ESP_OK;
  }

  ensure_builtin_routes();

  esp_err_t err = start_http_server();
  if (err != ESP_OK) {
    return err;
  }

  server_running = true;
  ESP_LOGI("gateway", "HTTP server started");

  return ESP_OK;
}

esp_err_t Gateway::stop_server() {
  if (!server_running) {
    ESP_LOGI("gateway", "Server already stopped");
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
  ESP_LOGI("gateway", "HTTP server stopped");
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

esp_err_t Gateway::handle_root_get(httpd_req_t *req) {
  (void)static_cast<Gateway *>(req->user_ctx);
  httpd_resp_set_type(req, html_content_type);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return send_embedded(req, ::_binary_index_html_start,
                       ::_binary_index_html_end);
}

esp_err_t Gateway::handle_app_js_get(httpd_req_t *req) {
  (void)static_cast<Gateway *>(req->user_ctx);
  httpd_resp_set_type(req, js_content_type);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return send_embedded(req, ::_binary_app_js_start, ::_binary_app_js_end);
}

esp_err_t Gateway::handle_assets_css_get(httpd_req_t *req) {
  (void)static_cast<Gateway *>(req->user_ctx);
  httpd_resp_set_type(req, css_content_type);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return send_embedded(req, ::_binary_index_css_start,
                       ::_binary_index_css_end);
}

esp_err_t Gateway::handle_device_info_get(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);

  esp_chip_info_t chip_info{};
  esp_chip_info(&chip_info);

  DeviceInfo device_info;
  device_info.model = chip_model_string(chip_info);
  device_info.firmware_version = gateway ? gateway->version() : "unknown";
  device_info.build_time = build_timestamp;
  device_info.idf_version = esp_get_idf_version();

  auto data = json_model::to_json(device_info);
  if (!data) {
    return ESP_ERR_NO_MEM;
  }

  return http::send_success(req, std::move(data));
}

esp_err_t Gateway::handle_wifi_credentials_post(httpd_req_t *req) {
  auto *gateway = static_cast<Gateway *>(req->user_ctx);
  if (!gateway) {
    return http::send_error(req, "Gateway unavailable", "gateway_unavailable");
  }

  if (req->content_len <= 0 ||
      static_cast<size_t>(req->content_len) > max_request_body_size) {
    return http::send_fail(req, "Invalid request size.");
  }

  std::string body;
  body.resize(static_cast<size_t>(req->content_len));
  size_t received = 0;

  while (received < body.size()) {
    const int ret =
        httpd_req_recv(req, body.data() + received, body.size() - received);
    if (ret <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      return http::send_fail(req, "Failed to read request body.");
    }
    received += static_cast<size_t>(ret);
  }

  auto root = json::parse(body);
  if (!root || !cJSON_IsObject(root.get())) {
    return http::send_fail(req, "Invalid JSON body.");
  }
  WifiCredentials credentials{};
  const char *bad_field = nullptr;
  if (!json_model::parse_wifi_credentials(root.get(), credentials, &bad_field)) {
    std::string message = bad_field
                            ? std::string(bad_field) + " must be a string."
                            : "Invalid credentials payload.";
    return http::send_fail_field(req, bad_field ? bad_field : "body", message.c_str());
  }

  if (credentials.ssid.empty() || credentials.ssid.size() > 32) {
    return http::send_fail_field(req, "ssid", "ssid must be 1-32 characters.");
  }

  if (!is_valid_passphrase(credentials.passphrase)) {
    return http::send_fail_field(req, "passphrase", "Passphrase must be 8-63 chars or 64 hex.");
  }

  ESP_LOGI("gateway", "Received Wi-Fi credentials update for SSID: %s", credentials.ssid.c_str());

  const esp_err_t result = gateway->save_wifi_credentials(credentials.ssid, credentials.passphrase);

  if (result != ESP_OK) {
    ESP_LOGE("gateway", "Failed to save Wi-Fi credentials: %s", esp_err_to_name(result));
    return http::send_error(req, "Failed to save credentials.", esp_err_to_name(result));
  }

  ESP_LOGI("gateway", "Wi-Fi credentials saved. Preparing to start station");

  esp_err_t sta_err = ESP_OK;
  bool sta_started = false;

  const esp_err_t stop_err = gateway->stop_station();
  if (stop_err != ESP_OK) {
    ESP_LOGW("gateway", "Failed to stop existing station: %s",
             esp_err_to_name(stop_err));
  }

  StationConfig station_cfg{};
  station_cfg.ssid = credentials.ssid;
  station_cfg.passphrase = credentials.passphrase;

  sta_err = gateway->start_station(station_cfg);
  if (sta_err == ESP_OK) {
    sta_started = true;
    ESP_LOGI("gateway", "Station connection initiated for SSID: %s",
             station_cfg.ssid.c_str());
  } else {
    ESP_LOGE("gateway", "Failed to start station: %s", esp_err_to_name(sta_err));
  }
  gateway->sta_autoconnect_attempted = true;

  auto data = json::object();
  if (!data) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "restart_required", !sta_started) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "sta_connect_started", sta_started) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (!sta_started) {
    if (json::add(data.get(), "sta_error", esp_err_to_name(sta_err)) != ESP_OK) {
      return ESP_ERR_NO_MEM;
    }
  }

  return http::send_success(req, std::move(data));
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

esp_err_t Gateway::save_wifi_credentials(std::string_view ssid,
                                         std::string_view passphrase) {
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(wifi_nvs_namespace, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  std::string ssid_copy(ssid);
  err = nvs_set_str(handle, wifi_nvs_ssid_key, ssid_copy.c_str());

  if (err == ESP_OK) {
    std::string pass_copy(passphrase);
    err = nvs_set_str(handle, wifi_nvs_pass_key, pass_copy.c_str());
  }

  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }

  nvs_close(handle);
  if (err == ESP_OK) {
    saved_sta_config.ssid.assign(ssid.data(), ssid.size());
    saved_sta_config.passphrase.assign(passphrase.data(), passphrase.size());
    has_saved_sta_credentials = !saved_sta_config.ssid.empty();
    sta_credentials_loaded = true;
    sta_autoconnect_attempted = false;
  }
  return err;
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

  ESP_LOGI("gateway", "mDNS started: host=%s instance=%s service=%s protocol=%s port=%u",
           mdns_config.hostname.c_str(), mdns_config.instance_name.c_str(),
           mdns_config.service_type.c_str(), mdns_config.protocol.c_str(),
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
