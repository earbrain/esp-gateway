#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace earbrain {

using RequestHandler = esp_err_t (*)(httpd_req_t *);

struct UriHandler {
  UriHandler(std::string_view path, httpd_method_t m, RequestHandler h,
             void *ctx);
  void refresh_descriptor();

  std::string uri;
  httpd_method_t method;
  RequestHandler handler;
  void *user_ctx;
  httpd_uri_t descriptor;
};

class HttpServer {
public:
  HttpServer() = default;
  ~HttpServer();

  esp_err_t start();
  esp_err_t stop();

  bool is_running() const noexcept { return running; }

  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, void *user_ctx = nullptr);
  bool has_route(std::string_view uri, httpd_method_t method) const;

private:
  esp_err_t register_route_with_server(UriHandler &route) const;

  httpd_handle_t handle = nullptr;
  bool running = false;
  std::vector<std::unique_ptr<UriHandler>> routes;
};

} // namespace earbrain
