cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS
        "src/gateway.cpp"
        "src/wifi_softap.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_http_server
    PRIV_REQUIRES
        esp_netif
        esp_event
        esp_wifi
        nvs_flash
        log
)
