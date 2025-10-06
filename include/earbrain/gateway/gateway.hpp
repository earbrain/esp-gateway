#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

struct esp_netif_obj;

namespace earbrain {

class Gateway {
public:
  Gateway();
  ~Gateway();

  esp_err_t start();
  esp_err_t stop();

  void set_softap_ssid(std::string_view ssid);

  using RequestHandler = esp_err_t (*)(httpd_req_t *);

  esp_err_t get(std::string_view uri, RequestHandler handler,
                void *user_ctx = nullptr);
  esp_err_t post(std::string_view uri, RequestHandler handler,
                 void *user_ctx = nullptr);

  const char *version() const { return "0.0.0"; }

private:
  struct UriHandler;

  esp_err_t start_softap();
  esp_err_t start_http_server();
  void ensure_builtin_routes();
  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, void *user_ctx);
  esp_err_t register_route_with_server(UriHandler &route);
  bool has_route(std::string_view uri, httpd_method_t method) const;
  static esp_err_t handle_root_get(httpd_req_t *req);
  static esp_err_t handle_app_js_get(httpd_req_t *req);
  static esp_err_t handle_assets_css_get(httpd_req_t *req);
  static esp_err_t handle_device_info_get(httpd_req_t *req);
  static esp_err_t handle_wifi_credentials_post(httpd_req_t *req);
  esp_err_t save_wifi_credentials(std::string_view ssid,
                                  std::string_view passphrase);
  char softap_ssid[33];
  std::size_t softap_ssid_len;
  esp_netif_obj *softap_netif;
  esp_netif_obj *sta_netif;
  httpd_handle_t http_server;
  bool softap_running;
  bool event_loop_created;
  bool builtin_routes_registered;
  std::vector<std::unique_ptr<UriHandler>> routes;
};

} // namespace earbrain
