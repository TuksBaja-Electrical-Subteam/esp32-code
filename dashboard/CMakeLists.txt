cmake_minimum_required(VERSION 3.16)

# Tell ESP-IDF where to find extra components (LVGL lives here)
set(EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/components   # standard ESP-IDF components
    ${CMAKE_SOURCE_DIR}/components  # your local components folder
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(BAJA_Speed_System)