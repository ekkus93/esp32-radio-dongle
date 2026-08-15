idf_component_register(
    SRCS "radio_uart_smoke.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart esp_timer radio_h4
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
