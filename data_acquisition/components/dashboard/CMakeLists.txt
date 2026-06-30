idf_component_register(
    SRCS 
        "main.c"
        "web_server.c"
        "gps_handler.c"
        "wifi_manager.c"
        "speed_calculator.c"
    INCLUDE_DIRS "."
    EMBED_FILES "index.html"
    REQUIRES 
        esp_http_server
        nvs_flash
        esp_wifi
        driver
        esp_netif
        esp_event
    PRIV_REQUIRES
        esp_timer
)