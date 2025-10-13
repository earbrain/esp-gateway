cmake_minimum_required(VERSION 3.16)

set(PORTAL_DIST_REL portal/dist)
set(PORTAL_DIST ${CMAKE_CURRENT_LIST_DIR}/${PORTAL_DIST_REL})
set(PORTAL_ASSETS
    ${CMAKE_CURRENT_LIST_DIR}/${PORTAL_DIST_REL}/index.html
    ${CMAKE_CURRENT_LIST_DIR}/${PORTAL_DIST_REL}/app.js
    ${CMAKE_CURRENT_LIST_DIR}/${PORTAL_DIST_REL}/assets/index.css
)

foreach(asset_abs IN LISTS PORTAL_ASSETS)
    if(NOT EXISTS ${asset_abs})
        message(FATAL_ERROR "${asset_abs} not found. Run 'npm run build' in the portal directory before building.")
    endif()
endforeach()

idf_component_register(
    SRCS
        "src/gateway.cpp"
        "src/handlers/device_handler.cpp"
        "src/handlers/health_handler.cpp"
        "src/handlers/log_handler.cpp"
        "src/handlers/metrics_handler.cpp"
        "src/handlers/mdns_handler.cpp"
        "src/handlers/portal_handler.cpp"
        "src/handlers/wifi_handler.cpp"
        "src/http_server.cpp"
        "src/logging.cpp"
        "src/mdns_service.cpp"
        "src/middlewares/logging.cpp"
        "src/validation.cpp"
        "src/wifi_service.cpp"
        "src/wifi_credentials.cpp"
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
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)

# Embed portal assets
idf_build_get_property(idf_path IDF_PATH)
set(DATA_EMBED_SCRIPT ${idf_path}/tools/cmake/scripts/data_file_embed_asm.cmake)
set(PORTAL_EMBED_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/portal_assets)
file(MAKE_DIRECTORY ${PORTAL_EMBED_OUTPUT_DIR})

set(PORTAL_EMBED_SOURCES)
foreach(asset_abs IN LISTS PORTAL_ASSETS)
    get_filename_component(asset_name ${asset_abs} NAME)
    set(embed_output ${PORTAL_EMBED_OUTPUT_DIR}/${asset_name}.S)
    add_custom_command(
        OUTPUT ${embed_output}
        COMMAND ${CMAKE_COMMAND}
                -D DATA_FILE=${asset_abs}
                -D SOURCE_FILE=${embed_output}
                -D FILE_TYPE=TEXT
                -P ${DATA_EMBED_SCRIPT}
        DEPENDS ${asset_abs}
        COMMENT "Embedding ${asset_name}"
        VERBATIM
    )
    list(APPEND PORTAL_EMBED_SOURCES ${embed_output})
endforeach()

# Add embedded sources to component library
target_sources(${COMPONENT_LIB} PRIVATE ${PORTAL_EMBED_SOURCES})

# Add version from idf_component.yml as compile definition
file(READ "${CMAKE_CURRENT_LIST_DIR}/idf_component.yml" IDF_COMPONENT_YML)
string(REGEX MATCH "version: *([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${IDF_COMPONENT_YML}")
set(GATEWAY_VERSION "${CMAKE_MATCH_1}")
target_compile_definitions(${COMPONENT_LIB} PUBLIC GATEWAY_VERSION="${GATEWAY_VERSION}")
message(STATUS "Set GATEWAY_VERSION to: ${GATEWAY_VERSION}")
