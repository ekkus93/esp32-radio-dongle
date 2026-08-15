cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "../../components/radio_h4"
    "../../components/radio_uart_smoke"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_wroom_uart_smoke)

if(NOT IDF_VERSION_MAJOR EQUAL 5 OR
   NOT IDF_VERSION_MINOR EQUAL 5 OR
   NOT IDF_VERSION_PATCH EQUAL 5)
    message(FATAL_ERROR
        "ESP32 Radio Dongle V1 requires ESP-IDF v5.5.5 exactly; found ${IDF_VER}")
endif()

if(NOT IDF_TARGET STREQUAL "esp32")
    message(FATAL_ERROR "esp32_wroom_uart_smoke must target esp32; found ${IDF_TARGET}")
endif()
