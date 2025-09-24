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
    EMBED_TXTFILES
        ${PORTAL_ASSETS}
)
