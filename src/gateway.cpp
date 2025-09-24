#include "earbrain/gateway/gateway.hpp"

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"

namespace earbrain {

using namespace std::string_view_literals;

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

    httpd_uri_t root{};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = &Gateway::handle_root_get;
    root.user_ctx = this;

    err = httpd_register_uri_handler(http_server, &root);
    if (err != ESP_OK) {
        httpd_stop(http_server);
        http_server = nullptr;
    }

    return err;
}

esp_err_t Gateway::handle_root_get(httpd_req_t *req) {
    static constexpr char portal_page_html[] = R"PORTAL(
<!DOCTYPE html><html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP Gateway</title>
</head><body>
<div class="wrapper">
  <h1>ESP Gateway</h1>
  <p>Hello World</p>
</div>
</body></html>)PORTAL";
    (void)static_cast<Gateway *>(req->user_ctx);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, portal_page_html, HTTPD_RESP_USE_STRLEN);
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
