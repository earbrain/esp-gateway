#include "earbrain/gateway/handlers/log_handler.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

#include "earbrain/logging.hpp"
#include "json/http_response.hpp"
#include "json/log_entries.hpp"

namespace earbrain::handlers::logs {

esp_err_t handle_get(httpd_req_t *req) {
  uint64_t cursor = 0;
  std::size_t limit = 100;

  const size_t query_len = httpd_req_get_url_query_len(req);
  if (query_len > 0 && query_len < 256) {
    std::string query(query_len + 1, '\0');
    if (httpd_req_get_url_query_str(req, query.data(), query.size()) == ESP_OK) {
      char buffer[32] = {0};

      if (httpd_query_key_value(query.c_str(), "cursor", buffer,
                                sizeof(buffer)) == ESP_OK) {
        char *end = nullptr;
        const unsigned long long parsed = strtoull(buffer, &end, 10);
        if (end && end != buffer) {
          cursor = parsed;
        }
      }

      if (httpd_query_key_value(query.c_str(), "limit", buffer,
                                sizeof(buffer)) == ESP_OK) {
        char *end = nullptr;
        const unsigned long parsed = strtoul(buffer, &end, 10);
        if (end && end != buffer) {
          constexpr std::size_t max_limit = logging::LogStore::max_entries;
          const std::size_t requested = static_cast<std::size_t>(parsed);
          limit =
              std::clamp<std::size_t>(requested, std::size_t{1}, max_limit);
        }
      }
    }
  }

  const logging::LogBatch batch = logging::collect(cursor, limit);
  auto data = json_model::to_json(batch);
  if (!data) {
    return http::send_error(req, "Failed to encode log entries");
  }

  return http::send_success(req, std::move(data));
}

} // namespace earbrain::handlers::logs

