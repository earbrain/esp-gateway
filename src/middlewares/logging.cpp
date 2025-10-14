#include "earbrain/gateway/middlewares/logging.hpp"

#include "earbrain/logging.hpp"
#include "esp_timer.h"
#include <cstring>

namespace earbrain::middleware {

namespace {

const char *http_method_str(int method) {
  switch (static_cast<httpd_method_t>(method)) {
  case HTTP_GET:
    return "GET";
  case HTTP_POST:
    return "POST";
  case HTTP_PUT:
    return "PUT";
  case HTTP_DELETE:
    return "DELETE";
  case HTTP_HEAD:
    return "HEAD";
  case HTTP_PATCH:
    return "PATCH";
  case HTTP_OPTIONS:
    return "OPTIONS";
  default:
    return "UNKNOWN";
  }
}

} // namespace

esp_err_t log_request(httpd_req_t *req, NextHandler next) {
  const char *method = http_method_str(req->method);
  const char *uri = req->uri;

  // Record start time
  int64_t start = esp_timer_get_time();

  // Execute the next handler
  esp_err_t result = next(req);

  // Calculate latency
  int64_t latency_us = esp_timer_get_time() - start;
  double latency_ms = latency_us / 1000.0;

  // Log request with response info
  const char *status = (result == ESP_OK) ? "200" : "error";

  logging::infof("http", "%s %s -> %s %.2fms",
                 method, uri, status, latency_ms);

  return result;
}

} // namespace earbrain::middleware
