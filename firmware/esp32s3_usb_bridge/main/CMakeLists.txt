idf_component_register(
    SRCS "main.c" "s3_bridge.c" "usb_descriptors.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart radio_h4 radio_usb_bth
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
