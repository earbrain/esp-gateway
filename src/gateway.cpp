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
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"
#include <cJSON.h>

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
static constexpr auto json_content_type = "application/json";
static constexpr size_t max_request_body_size = 1024;
static constexpr const char wifi_nvs_namespace[] = "wifi";
static constexpr const char wifi_nvs_ssid_key[] = "sta_ssid";
static constexpr const char wifi_nvs_pass_key[] = "sta_pass";

static esp_err_t send_json_response(httpd_req_t *req, cJSON *json) {
    char *buffer = cJSON_PrintUnformatted(json);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    httpd_resp_set_type(req, json_content_type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const esp_err_t err = httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
    cJSON_free(buffer);
    return err;
}

static esp_err_t send_response(httpd_req_t *req, const char *status,
                               cJSON *data = nullptr,
                               const char *error_message = nullptr,
                               const char *http_status = nullptr) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddStringToObject(root, "status", status)) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    cJSON *data_obj = data ? data : cJSON_CreateObject();
    if (!data_obj) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "data", data_obj);

    cJSON *error_node = nullptr;
    if (error_message) {
        error_node = cJSON_CreateString(error_message);
    } else {
        error_node = cJSON_CreateNull();
    }
    if (!error_node) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "error", error_node);

    if (http_status) {
        httpd_resp_set_status(req, http_status);
    }

    const esp_err_t err = send_json_response(req, root);
    cJSON_Delete(root);
    return err;
}

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
      http_server(nullptr), softap_running(false), event_loop_created(false),
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

    esp_netif_obj *new_netif = esp_netif_create_default_wifi_ap();
    if (!new_netif) {
        return ESP_FAIL;
    }

    err = start_softap();
    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(new_netif);
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
        esp_netif_destroy_default_wifi(new_netif);
        if (event_loop_created) {
            esp_event_loop_delete_default();
            event_loop_created = false;
        }
        return err;
    }

    softap_netif = new_netif;
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

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddStringToObject(data, "model", device_info.model.c_str()) ||
        !cJSON_AddStringToObject(data, "firmware_version",
                                 device_info.firmware_version.c_str()) ||
        !cJSON_AddStringToObject(data, "build_time",
                                 device_info.build_time.c_str()) ||
        !cJSON_AddStringToObject(data, "idf_version",
                                 device_info.idf_version.c_str())) {
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }

    return send_response(req, "success", data);
}

esp_err_t Gateway::handle_wifi_credentials_post(httpd_req_t *req) {
    auto *gateway = static_cast<Gateway *>(req->user_ctx);
    if (!gateway) {
        cJSON *error_obj = cJSON_CreateObject();
        if (!error_obj) {
            return ESP_ERR_NO_MEM;
        }

        if (!cJSON_AddStringToObject(error_obj, "message",
                                     "Gateway unavailable") ||
            !cJSON_AddStringToObject(error_obj, "code",
                                     "gateway_unavailable")) {
            cJSON_Delete(error_obj);
            return ESP_ERR_NO_MEM;
        }

        return send_response(req, "error", nullptr, "Gateway unavailable",
                             "500 Internal Server Error");
    }

    if (req->content_len <= 0 ||
        static_cast<size_t>(req->content_len) > max_request_body_size) {
        return send_response(req,
                             "fail", nullptr, "Invalid request size.",
                             "400 Bad Request");
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
            return send_response(req, "fail", nullptr,
                                 "Failed to read request body.",
                                 "400 Bad Request");
        }
        received += static_cast<size_t>(ret);
    }

    cJSON *root = cJSON_ParseWithLength(body.c_str(), body.size());
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return send_response(req, "fail", nullptr, "Invalid JSON body.",
                             "400 Bad Request");
    }

    esp_err_t result = ESP_FAIL;

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == nullptr) {
        cJSON_Delete(root);
        cJSON *data = cJSON_CreateObject();
        if (!data || !cJSON_AddStringToObject(data, "field", "ssid")) {
            cJSON_Delete(data);
            return ESP_ERR_NO_MEM;
        }
        return send_response(req, "fail", data, "ssid must be a string.",
                             "400 Bad Request");
    }

    std::string ssid = ssid_item->valuestring;
    if (ssid.empty() || ssid.size() > 32) {
        cJSON_Delete(root);
        cJSON *data = cJSON_CreateObject();
        if (!data || !cJSON_AddStringToObject(data, "field", "ssid")) {
            cJSON_Delete(data);
            return ESP_ERR_NO_MEM;
        }
        return send_response(req, "fail", data,
                             "ssid must be 1-32 characters.",
                             "400 Bad Request");
    }

    cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(root, "passphrase");
    if (!pass_item || !cJSON_IsString(pass_item) ||
        pass_item->valuestring == nullptr) {
        cJSON_Delete(root);
        cJSON *data = cJSON_CreateObject();
        if (!data ||
            !cJSON_AddStringToObject(data, "field", "passphrase")) {
            cJSON_Delete(data);
            return ESP_ERR_NO_MEM;
        }
        return send_response(req, "fail", data,
                             "passphrase must be a string.",
                             "400 Bad Request");
    }

    std::string passphrase = pass_item->valuestring;
    if (!is_valid_passphrase(passphrase)) {
        cJSON_Delete(root);
        cJSON *data = cJSON_CreateObject();
        if (!data ||
            !cJSON_AddStringToObject(data, "field", "passphrase")) {
            cJSON_Delete(data);
            return ESP_ERR_NO_MEM;
        }
        return send_response(req, "fail", data,
                             "Passphrase must be 8-63 chars or 64 hex.",
                             "400 Bad Request");
    }

    ESP_LOGI("gateway", "Received Wi-Fi credentials update for SSID: %s",
             ssid.c_str());

    result = gateway->save_wifi_credentials(ssid, passphrase);
    cJSON_Delete(root);

    if (result != ESP_OK) {
        ESP_LOGE("gateway", "Failed to save Wi-Fi credentials: %s",
                 esp_err_to_name(result));
        cJSON *data = cJSON_CreateObject();
        if (data) {
            cJSON_AddStringToObject(data, "detail", esp_err_to_name(result));
        }
        return send_response(req, "error", data,
                             "Failed to save credentials.",
                             "500 Internal Server Error");
    }

    ESP_LOGI("gateway", "Wi-Fi credentials saved. Restart required: true");

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddBoolToObject(data, "restart_required", true)) {
        cJSON_Delete(data);
        return ESP_ERR_NO_MEM;
    }

    return send_response(req, "success", data);
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
