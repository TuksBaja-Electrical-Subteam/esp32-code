cmake_minimum_required(VERSION 3.16)

# Glob all LVGL source files recursively
file(GLOB_RECURSE LVGL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src/*.c
)

idf_component_register(
    SRCS
        ${LVGL_SOURCES}
    INCLUDE_DIRS
        "."          # exposes components/lvgl/ so #include "lvgl.h" works
        "lvgl"       # exposes components/lvgl/lvgl/ for internal includes
    REQUIRES
        esp_timer
)

# Point LVGL to your config file (lv_conf.h)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LV_CONF_INCLUDE_SIMPLE=1
)
