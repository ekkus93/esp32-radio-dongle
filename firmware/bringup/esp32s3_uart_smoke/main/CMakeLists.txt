idf_component_register(
    SRCS "main.c"
    REQUIRES radio_uart_smoke
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
