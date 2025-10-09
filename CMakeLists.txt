cmake_minimum_required(VERSION 3.16)

set(PORTAL_DIST_REL portal/dist)
set(PORTAL_DIST ${CMAKE_CURRENT_LIST_DIR}/${PORTAL_DIST_REL})
set(PORTAL_ASSETS
    ${PORTAL_DIST_REL}/index.html
    ${PORTAL_DIST_REL}/app.js
    ${PORTAL_DIST_REL}/assets/index.css
)

foreach(asset_rel IN LISTS PORTAL_ASSETS)
    set(asset_abs ${CMAKE_CURRENT_LIST_DIR}/${asset_rel})
    if(NOT EXISTS ${asset_abs})
        message(FATAL_ERROR "${asset_rel} not found. Run 'npm run build' in the portal directory before building.")
    endif()
endforeach()

idf_component_register(
    SRCS
        "src/gateway.cpp"
        "src/handlers/device_handler.cpp"
        "src/handlers/log_handler.cpp"
        "src/handlers/metrics_handler.cpp"
        "src/handlers/mdns_handler.cpp"
        "src/handlers/portal_handler.cpp"
        "src/handlers/wifi_handler.cpp"
        "src/logging.cpp"
        "src/wifi.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        esp_http_server
        json
        esp_wifi
    PRIV_REQUIRES
        esp_netif
        esp_event
        nvs_flash
        log
        mdns
        esp_timer
        heap
    EMBED_TXTFILES
        ${PORTAL_ASSETS}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
