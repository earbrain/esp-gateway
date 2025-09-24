get_filename_component(_ESP_GATEWAY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)

idf_component_register(
    SRCS
        "${_ESP_GATEWAY_ROOT}/src/gateway.cpp"
    INCLUDE_DIRS
        "${_ESP_GATEWAY_ROOT}/include"
)
