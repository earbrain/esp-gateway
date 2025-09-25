#include "earbrain/gateway/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

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

static esp_err_t send_error(httpd_req_t *req, const char *status,
                            const char *message, const char *field = nullptr) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "result", "error");
    cJSON_AddStringToObject(root, "message", message);
    if (field) {
        cJSON_AddStringToObject(root, "field", field);
    }

    httpd_resp_set_status(req, status);
    esp_err_t err = send_json_response(req, root);
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

Gateway::Gateway()
    : softap_ssid{}, softap_ssid_len(0), softap_netif(nullptr),
      http_server(nullptr), softap_running(false), event_loop_created(false) {
    set_softap_ssid("gateway-ap"sv);
}

Gateway::~Gateway() = default;

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

    static httpd_uri_t root_uri_handler = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = &Gateway::handle_root_get,
        .user_ctx = nullptr,
    };
    root_uri_handler.user_ctx = this;
    err = httpd_register_uri_handler(http_server, &root_uri_handler);
    if (err != ESP_OK) {
        httpd_stop(http_server);
        http_server = nullptr;
    }

    if (err == ESP_OK) {
        static httpd_uri_t app_js_uri_handler = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = &Gateway::handle_app_js_get,
            .user_ctx = nullptr,
        };
        app_js_uri_handler.user_ctx = this;
        err = httpd_register_uri_handler(http_server, &app_js_uri_handler);
    }

    if (err == ESP_OK) {
        static httpd_uri_t css_uri_handler = {
            .uri = "/assets/index.css",
            .method = HTTP_GET,
            .handler = &Gateway::handle_assets_css_get,
            .user_ctx = nullptr,
        };
        css_uri_handler.user_ctx = this;
        err = httpd_register_uri_handler(http_server, &css_uri_handler);
    }

    if (err == ESP_OK) {
        static httpd_uri_t device_info_handler = {
            .uri = "/api/v1/device-info",
            .method = HTTP_GET,
            .handler = &Gateway::handle_device_info_get,
            .user_ctx = nullptr,
        };
        device_info_handler.user_ctx = this;
        err = httpd_register_uri_handler(http_server, &device_info_handler);
    }

    if (err == ESP_OK) {
        static httpd_uri_t wifi_credentials_handler = {
            .uri = "/api/v1/wifi/credentials",
            .method = HTTP_POST,
            .handler = &Gateway::handle_wifi_credentials_post,
            .user_ctx = nullptr,
        };
        wifi_credentials_handler.user_ctx = this;
        err =
            httpd_register_uri_handler(http_server, &wifi_credentials_handler);
    }

    return err;
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

    const std::string json_payload = device_info.to_json();

    httpd_resp_set_type(req, json_content_type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json_payload.c_str(), json_payload.size());
}

esp_err_t Gateway::handle_wifi_credentials_post(httpd_req_t *req) {
    auto *gateway = static_cast<Gateway *>(req->user_ctx);
    if (!gateway) {
        return send_error(req, "500 Internal Server Error",
                          "Gateway unavailable");
    }

    if (req->content_len <= 0 ||
        static_cast<size_t>(req->content_len) > max_request_body_size) {
        return send_error(req, "400 Bad Request", "Invalid request size");
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
            return send_error(req, "400 Bad Request",
                              "Failed to read request body");
        }
        received += static_cast<size_t>(ret);
    }

    cJSON *root = cJSON_ParseWithLength(body.c_str(), body.size());
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return send_error(req, "400 Bad Request", "Invalid JSON body");
    }

    esp_err_t result = ESP_FAIL;

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == nullptr) {
        cJSON_Delete(root);
        return send_error(req, "400 Bad Request", "ssid must be a string",
                          "ssid");
    }

    std::string ssid = ssid_item->valuestring;
    if (ssid.empty() || ssid.size() > 32) {
        cJSON_Delete(root);
        return send_error(req, "400 Bad Request",
                          "ssid must be 1-32 characters", "ssid");
    }

    cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(root, "passphrase");
    if (!pass_item || !cJSON_IsString(pass_item) ||
        pass_item->valuestring == nullptr) {
        cJSON_Delete(root);
        return send_error(req, "400 Bad Request",
                          "passphrase must be provided as a string",
                          "passphrase");
    }

    std::string passphrase = pass_item->valuestring;
    if (!is_valid_passphrase(passphrase)) {
        cJSON_Delete(root);
        return send_error(req, "400 Bad Request",
                          "passphrase must be 8-63 chars or 64 hex",
                          "passphrase");
    }

    ESP_LOGI("gateway", "Received Wi-Fi credentials update for SSID: %s",
             ssid.c_str());

    result = gateway->save_wifi_credentials(ssid, passphrase);
    cJSON_Delete(root);

    if (result != ESP_OK) {
        ESP_LOGE("gateway", "Failed to save Wi-Fi credentials: %s",
                 esp_err_to_name(result));
        return send_error(req, "500 Internal Server Error",
                          "Failed to save credentials");
    }

    cJSON *response = cJSON_CreateObject();
    if (!response) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI("gateway", "Wi-Fi credentials saved. Restart required: true");

    cJSON_AddStringToObject(response, "result", "ok");
    cJSON_AddBoolToObject(response, "restart_required", true);

    const esp_err_t send_result = send_json_response(req, response);
    cJSON_Delete(response);
    return send_result;
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
