idf_component_register(
    SRCS "display.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd driver ui wheel_speed data_types freertos esp_timer heap
)