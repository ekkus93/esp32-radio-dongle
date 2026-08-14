cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3_usb_bridge)

if(NOT IDF_VERSION_MAJOR EQUAL 5 OR
   NOT IDF_VERSION_MINOR EQUAL 5 OR
   NOT IDF_VERSION_PATCH EQUAL 5)
    message(FATAL_ERROR
        "ESP32 Radio Dongle V1 requires ESP-IDF v5.5.5 exactly; found ${IDF_VER}")
endif()

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR
        "esp32s3_usb_bridge must target esp32s3; found ${IDF_TARGET}")
endif()
