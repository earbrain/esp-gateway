#include "earbrain/gateway/http_server.hpp"

namespace earbrain {

UriHandler::UriHandler(std::string_view path, httpd_method_t m,
                       RequestHandler h, void *ctx)
  : uri(path), method(m), handler(h), user_ctx(ctx), descriptor{} {
  refresh_descriptor();
}

void UriHandler::refresh_descriptor() {
  descriptor.uri = uri.c_str();
  descriptor.method = method;
  descriptor.handler = handler;
  descriptor.user_ctx = user_ctx;
}

HttpServer::~HttpServer() {
  stop();
}

esp_err_t HttpServer::start() {
  if (running) {
    return ESP_OK;
  }

  if (handle) {
    httpd_stop(handle);
    handle = nullptr;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  config.lru_purge_enable = true;
  // Allow slower clients more time before the server aborts the socket on send/recv.
  config.recv_wait_timeout = 20;
  config.send_wait_timeout = 30;

  esp_err_t err = httpd_start(&handle, &config);
  if (err != ESP_OK) {
    handle = nullptr;
    return err;
  }

  for (auto &route : routes) {
    const esp_err_t reg_err = register_route_with_server(*route);
    if (reg_err != ESP_OK) {
      httpd_stop(handle);
      handle = nullptr;
      return reg_err;
    }
  }

  running = true;
  return ESP_OK;
}

esp_err_t HttpServer::stop() {
  if (!running) {
    return ESP_OK;
  }

  if (handle) {
    esp_err_t err = httpd_stop(handle);
    if (err != ESP_OK) {
      return err;
    }
    handle = nullptr;
  }

  running = false;
  return ESP_OK;
}

esp_err_t HttpServer::add_route(std::string_view uri, httpd_method_t method,
                                RequestHandler handler, void *user_ctx) {
  if (uri.empty() || !handler) {
    return ESP_ERR_INVALID_ARG;
  }

  if (has_route(uri, method)) {
    return ESP_ERR_INVALID_STATE;
  }

  auto entry = std::make_unique<UriHandler>(uri, method, handler, user_ctx);
  UriHandler &route = *entry;

  if (handle) {
    const esp_err_t err = register_route_with_server(route);
    if (err != ESP_OK) {
      return err;
    }
  }

  routes.push_back(std::move(entry));
  return ESP_OK;
}

bool HttpServer::has_route(std::string_view uri, httpd_method_t method) const {
  for (const auto &route : routes) {
    if (route->method == method && route->uri == uri) {
      return true;
    }
  }
  return false;
}

esp_err_t HttpServer::register_route_with_server(UriHandler &route) const {
  if (!handle) {
    return ESP_ERR_INVALID_STATE;
  }

  route.refresh_descriptor();
  return httpd_register_uri_handler(handle, &route.descriptor);
}

} // namespace earbrain
