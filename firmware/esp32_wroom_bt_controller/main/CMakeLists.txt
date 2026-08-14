idf_component_register(
    SRCS "main.c" "wroom_bridge.c"
    INCLUDE_DIRS "."
    REQUIRES bt esp_driver_uart nvs_flash radio_h4
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
