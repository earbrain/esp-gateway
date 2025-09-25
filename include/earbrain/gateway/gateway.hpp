#pragma once

#include <cstddef>
#include <string_view>
#include "esp_err.h"
#include "esp_http_server.h"

struct esp_netif_obj;

namespace earbrain {

class Gateway {
public:
  Gateway();
  ~Gateway();

  esp_err_t start();
  esp_err_t stop();

  void set_softap_ssid(std::string_view ssid);

  const char *version() const { return "0.0.0"; }

private:
  esp_err_t start_softap();
  esp_err_t start_http_server();
  static esp_err_t handle_root_get(httpd_req_t *req);
  static esp_err_t handle_app_js_get(httpd_req_t *req);
  static esp_err_t handle_assets_css_get(httpd_req_t *req);
  static esp_err_t handle_device_info_get(httpd_req_t *req);
  char softap_ssid[33];
  std::size_t softap_ssid_len;
  esp_netif_obj *softap_netif;
  httpd_handle_t http_server;
  bool softap_running;
  bool event_loop_created;
};

} // namespace earbrain
