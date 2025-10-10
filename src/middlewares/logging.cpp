#include "earbrain/gateway/middlewares/logging.hpp"

#include "earbrain/gateway/logging.hpp"
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

esp_err_t log_request(httpd_req_t *req) {
  const char *method = http_method_str(req->method);

  char query[128] = {0};
  bool has_query = (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK);

  char body[256] = {0};
  size_t content_len = req->content_len;
  if (content_len > 0 && content_len < sizeof(body)) {
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
      body[0] = '\0';
    } else {
      body[ret] = '\0';
    }
  }

  if (has_query && body[0] != '\0') {
    logging::infof("http", "%s %s?%s body=%s", method, req->uri, query, body);
  } else if (has_query) {
    logging::infof("http", "%s %s?%s", method, req->uri, query);
  } else if (body[0] != '\0') {
    logging::infof("http", "%s %s body=%s", method, req->uri, body);
  } else {
    logging::infof("http", "%s %s", method, req->uri);
  }

  return ESP_OK;
}

} // namespace earbrain::middleware
