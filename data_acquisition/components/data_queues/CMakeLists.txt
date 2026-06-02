idf_component_register(
    SRCS "data_queues.c"
    INCLUDE_DIRS "include"
    REQUIRES data_types driver freertos esp_timer
)