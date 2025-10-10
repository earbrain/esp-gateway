#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace earbrain {

using RequestHandler = esp_err_t (*)(httpd_req_t *);
using Middleware = esp_err_t (*)(httpd_req_t *);

struct RouteOptions {
  std::vector<Middleware> middlewares;
  void *user_ctx = nullptr;
};

class HttpServer;

struct UriHandler {
  UriHandler(std::string_view path, httpd_method_t m, RequestHandler h,
             void *ctx, HttpServer *srv);
  UriHandler(std::string_view path, httpd_method_t m, RequestHandler h,
             const RouteOptions &opts, HttpServer *srv);
  void refresh_descriptor();

  std::string uri;
  httpd_method_t method;
  RequestHandler handler;
  void *user_ctx;
  httpd_uri_t descriptor;
  std::vector<Middleware> middlewares;
  HttpServer *server;
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
  esp_err_t add_route(std::string_view uri, httpd_method_t method,
                      RequestHandler handler, const RouteOptions &options);
  bool has_route(std::string_view uri, httpd_method_t method) const;

  // Global middleware management
  void use(Middleware middleware);
  const std::vector<Middleware> &get_global_middlewares() const noexcept {
    return global_middlewares;
  }

private:
  esp_err_t register_route_with_server(UriHandler &route) const;

  httpd_handle_t handle = nullptr;
  bool running = false;
  std::vector<std::unique_ptr<UriHandler>> routes;
  std::vector<Middleware> global_middlewares;
};

} // namespace earbrain
