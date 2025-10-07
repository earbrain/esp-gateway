#pragma once

#include "json/json_helpers.hpp"

#include "esp_http_server.h"
#include "esp_log.h"

namespace earbrain::http {

inline esp_err_t send_json_response(httpd_req_t *req, cJSON *json) {
  if (!req || !json) {
    return ESP_ERR_INVALID_ARG;
  }

  char *buffer = cJSON_PrintUnformatted(json);
  if (!buffer) {
    return ESP_ERR_NO_MEM;
  }

#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG
  ESP_LOGD("gateway", "response: %s", buffer);
#endif
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  const esp_err_t err = httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
  cJSON_free(buffer);
  return err;
}

inline esp_err_t send_response(httpd_req_t *req, const char *status,
                               json::Ptr data = json::Ptr{},
                               const char *error_message = nullptr,
                               const char *http_status = nullptr) {
  if (!req || !status) {
    return ESP_ERR_INVALID_ARG;
  }

  auto root = json::object();
  if (!root) {
    return ESP_ERR_NO_MEM;
  }

  if (json::add(root.get(), "status", status) != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  cJSON *data_obj = nullptr;
  if (data) {
    data_obj = data.release();
  } else {
    auto empty = json::object();
    if (!empty) {
      return ESP_ERR_NO_MEM;
    }
    data_obj = empty.release();
  }
  cJSON_AddItemToObject(root.get(), "data", data_obj);

  json::Ptr error_node{error_message
                         ? cJSON_CreateString(error_message)
                         : cJSON_CreateNull()};
  if (!error_node) {
    return ESP_ERR_NO_MEM;
  }
  cJSON_AddItemToObject(root.get(), "error", error_node.release());

  if (http_status) {
    httpd_resp_set_status(req, http_status);
  }

  const esp_err_t err = send_json_response(req, root.get());
  return err;
}

inline esp_err_t send_success(httpd_req_t *req, json::Ptr data = json::Ptr{},
                              const char *http_status = nullptr) {
  return send_response(req, "success", std::move(data), nullptr,
                       http_status);
}

inline esp_err_t send_fail_field(httpd_req_t *req, const char *field,
                                 const char *message,
                                 const char *http_status =
                                     "400 Bad Request") {
  auto data = json::object();
  if (!data) {
    return ESP_ERR_NO_MEM;
  }
  if (json::add(data.get(), "field", field ? field : "") != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  return send_response(req, "fail", std::move(data), message, http_status);
}

inline esp_err_t send_error(httpd_req_t *req, const char *message,
                            const char *detail = nullptr,
                            const char *http_status =
                                "500 Internal Server Error") {
  json::Ptr data;
  if (detail) {
    data = json::object();
    if (!data) {
      return ESP_ERR_NO_MEM;
    }
    if (json::add(data.get(), "detail", detail) != ESP_OK) {
      return ESP_ERR_NO_MEM;
    }
  }
  return send_response(req, "error", std::move(data), message, http_status);
}

inline esp_err_t send_fail(httpd_req_t *req, const char *message, const char *http_status = "400 Bad Request") {
  return send_response(req, "fail", json::Ptr{}, message, http_status);
}

} // namespace earbrain::http
