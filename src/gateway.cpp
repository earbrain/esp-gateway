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
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"

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
static constexpr const char wifi_nvs_namespace[] = "wifi";
static constexpr const char wifi_nvs_ssid_key[] = "sta_ssid";
static constexpr const char wifi_nvs_pass_key[] = "sta_pass";

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
      sta_netif(nullptr), http_server(nullptr), softap_running(false),
      event_loop_created(false),
      builtin_routes_registered(false), routes() {
    set_softap_ssid("gateway-ap"sv);
}

Gateway::~Gateway() = default;

esp_err_t Gateway::get(std::string_view uri, RequestHandler handler,
                       void *user_ctx) {
    return add_route(uri, HTTP_GET, handler, user_ctx ? user_ctx : this);
}

esp_err_t Gateway::post(std::string_view uri, RequestHandler handler,
                        void *user_ctx) {
    return add_route(uri, HTTP_POST, handler, user_ctx ? user_ctx : this);
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

    auto entry = std::make_unique<UriHandler>(uri, method, handler, user_ctx);
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

esp_err_t Gateway::register_route_with_server(UriHandler &route) {
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
        {"/app.js", HTTP_GET, &Gateway::handle_app_js_get},
        {"/assets/index.css", HTTP_GET, &Gateway::handle_assets_css_get},
        {"/api/v1/device-info", HTTP_GET, &Gateway::handle_device_info_get},
        {"/api/v1/wifi/credentials", HTTP_POST,
         &Gateway::handle_wifi_credentials_post},
    };

    for (const auto &route : routes_to_register) {
        esp_err_t err = ESP_OK;
        if (route.method == HTTP_POST) {
            err = post(route.uri, route.handler, this);
        } else {
            err = get(route.uri, route.handler, this);
        }

        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW("gateway", "Failed to register builtin route %.*s: %s",
                     static_cast<int>(route.uri.size()), route.uri.data(),
                     esp_err_to_name(err));
        }
    }
}

esp_err_t Gateway::start() {
    if (softap_running) {
        ESP_LOGI("gateway", "SoftAP already running");
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err == ESP_OK) {
        event_loop_created = true;
    } else if (err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (softap_netif) {
        esp_netif_destroy_default_wifi(softap_netif);
        softap_netif = nullptr;
    }

    if (sta_netif) {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = nullptr;
    }

    esp_netif_obj *new_ap_netif = esp_netif_create_default_wifi_ap();
    if (!new_ap_netif) {
        return ESP_FAIL;
    }

    esp_netif_obj *new_sta_netif = esp_netif_create_default_wifi_sta();
    if (!new_sta_netif) {
        esp_netif_destroy_default_wifi(new_ap_netif);
        return ESP_FAIL;
    }

    err = start_softap();
    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(new_sta_netif);
        esp_netif_destroy_default_wifi(new_ap_netif);
        if (event_loop_created) {
            esp_event_loop_delete_default();
            event_loop_created = false;
        }
        return err;
    }

    ensure_builtin_routes();

    err = start_http_server();
    if (err != ESP_OK) {
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(new_sta_netif);
        esp_netif_destroy_default_wifi(new_ap_netif);
        if (event_loop_created) {
            esp_event_loop_delete_default();
            event_loop_created = false;
        }
        return err;
    }

    softap_netif = new_ap_netif;
    sta_netif = new_sta_netif;
    ESP_LOGI("gateway", "SoftAP ready");
    softap_running = true;
    return ESP_OK;
}

esp_err_t Gateway::stop() {
    if (!softap_running) {
        ESP_LOGI("gateway", "SoftAP already stopped");
        return ESP_OK;
    }

    if (http_server) {
        httpd_stop(http_server);
        http_server = nullptr;
    }

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT &&
        err != ESP_ERR_WIFI_NOT_STARTED) {
        return err;
    }

    err = esp_wifi_deinit();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        return err;
    }

    if (softap_netif) {
        esp_netif_destroy_default_wifi(softap_netif);
        softap_netif = nullptr;
    }

    if (sta_netif) {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = nullptr;
    }

    if (event_loop_created) {
        esp_event_loop_delete_default();
        event_loop_created = false;
    }

    softap_running = false;
    return ESP_OK;
}

esp_err_t Gateway::start_http_server() {
    if (http_server) {
        httpd_stop(http_server);
        http_server = nullptr;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
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

    ESP_LOGI("gateway", "Wi-Fi credentials saved. Restart required: true");

    auto data = json::object();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    if (json::add(data.get(), "restart_required", true) != ESP_OK) {
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
    return err;
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
}

} // namespace earbrain
