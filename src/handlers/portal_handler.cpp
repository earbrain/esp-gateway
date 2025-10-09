#include "earbrain/gateway/handlers/portal_handler.hpp"

#include <cstddef>

#include "esp_http_server.h"

namespace earbrain::handlers::portal {

namespace {

constexpr auto html_content_type = "text/html; charset=utf-8";
constexpr auto js_content_type = "application/javascript";
constexpr auto css_content_type = "text/css";
constexpr size_t chunk_size = 1024;

constexpr const uint8_t *truncate_null_terminator(const uint8_t *begin,
                                                   const uint8_t *end) {
  return (begin != end && *(end - 1) == '\0') ? end - 1 : end;
}

esp_err_t send_embedded(httpd_req_t *req, const uint8_t *begin,
                        const uint8_t *end) {
  end = truncate_null_terminator(begin, end);
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

} // namespace

extern "C" {
extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];
extern const uint8_t _binary_app_js_start[];
extern const uint8_t _binary_app_js_end[];
extern const uint8_t _binary_index_css_start[];
extern const uint8_t _binary_index_css_end[];
}

esp_err_t handle_root_get(httpd_req_t *req) {
  httpd_resp_set_type(req, html_content_type);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return send_embedded(req, _binary_index_html_start, _binary_index_html_end);
}

esp_err_t handle_app_js_get(httpd_req_t *req) {
  httpd_resp_set_type(req, js_content_type);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return send_embedded(req, _binary_app_js_start, _binary_app_js_end);
}

esp_err_t handle_assets_css_get(httpd_req_t *req) {
  httpd_resp_set_type(req, css_content_type);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return send_embedded(req, _binary_index_css_start, _binary_index_css_end);
}

} // namespace earbrain::handlers::portal
