#pragma once

#include <cstddef>
#include <string_view>

#include "esp_err.h"

struct esp_netif_obj;

namespace earbrain {

class Gateway {
public:
  Gateway();
  ~Gateway();

  esp_err_t start();
  esp_err_t stop();

  void set_softap_ssid(std::string_view ssid);

  const char *version() const { return "0.0.0"; }

private:
  esp_err_t start_softap();
  char softap_ssid[33];
  std::size_t softap_ssid_len;
  esp_netif_obj *softap_netif;
  bool softap_running;
  bool event_loop_created;
};

} // namespace earbrain
