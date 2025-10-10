#include "earbrain/gateway/http_server.hpp"

namespace earbrain {

namespace {

esp_err_t middleware_wrapper(httpd_req_t *req) {
  auto *route = static_cast<UriHandler *>(req->user_ctx);
  if (!route) {
    return ESP_FAIL;
  }

  // Create request context
  RequestContext ctx;
  ctx.gateway = static_cast<Gateway *>(route->user_ctx);
  req->user_ctx = &ctx;

  // Build the handler chain
  NextHandler next = [route](httpd_req_t *r) {
    return route->handler(r);
  };

  for (auto it = route->middlewares.rbegin(); it != route->middlewares.rend(); ++it) {
    const Middleware &middleware = *it;
    NextHandler current_next = next;
    next = [middleware, current_next](httpd_req_t *r) {
      return middleware(r, current_next);
    };
  }

  if (route->server) {
    const auto &global_middlewares = route->server->get_global_middlewares();
    for (auto it = global_middlewares.rbegin(); it != global_middlewares.rend(); ++it) {
      const Middleware &middleware = *it;
      NextHandler current_next = next;
      next = [middleware, current_next](httpd_req_t *r) {
        return middleware(r, current_next);
      };
    }
  }

  // Execute the chain - ctx must stay in scope during this call
  esp_err_t result = next(req);
  return result;
}

} // namespace

UriHandler::UriHandler(std::string_view path, httpd_method_t m,
                       RequestHandler h, void *ctx, HttpServer *srv)
  : uri(path), method(m), handler(h), user_ctx(ctx), descriptor{},
    middlewares{}, server(srv) {
  refresh_descriptor();
}

UriHandler::UriHandler(std::string_view path, httpd_method_t m,
                       RequestHandler h, const RouteOptions &opts, HttpServer *srv)
  : uri(path), method(m), handler(h), user_ctx(opts.user_ctx),
    descriptor{}, middlewares(opts.middlewares), server(srv) {
  refresh_descriptor();
}

void UriHandler::refresh_descriptor() {
  descriptor.uri = uri.c_str();
  descriptor.method = method;

  bool has_middlewares = !middlewares.empty();
  if (!has_middlewares && server) {
    has_middlewares = !server->get_global_middlewares().empty();
  }

  if (has_middlewares) {
    descriptor.handler = &middleware_wrapper;
    descriptor.user_ctx = this;
  } else {
    descriptor.handler = handler;
    descriptor.user_ctx = user_ctx;
  }
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
  config.max_uri_handlers = 32;
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

  auto entry = std::make_unique<UriHandler>(uri, method, handler, user_ctx, this);
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

esp_err_t HttpServer::add_route(std::string_view uri, httpd_method_t method,
                                RequestHandler handler, const RouteOptions &options) {
  if (uri.empty() || !handler) {
    return ESP_ERR_INVALID_ARG;
  }

  if (has_route(uri, method)) {
    return ESP_ERR_INVALID_STATE;
  }

  auto entry = std::make_unique<UriHandler>(uri, method, handler, options, this);
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

void HttpServer::use(Middleware middleware) {
  global_middlewares.push_back(middleware);
  for (auto &route : routes) {
    if (handle) {
      httpd_unregister_uri_handler(handle, route->uri.c_str(), route->method);
      register_route_with_server(*route);
    } else {
      route->refresh_descriptor();
    }
  }
}

// Helper function for request context
RequestContext *get_request_context(httpd_req_t *req) {
  return static_cast<RequestContext *>(req->user_ctx);
}

} // namespace earbrain
