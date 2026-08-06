idf_component_register(
    SRCS "wheel_speed.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos esp_timer data_types data_queues esp_driver_gpio
)