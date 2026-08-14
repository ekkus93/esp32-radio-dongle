idf_component_register(
    SRCS "radio_h4.c"
    INCLUDE_DIRS "include"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
