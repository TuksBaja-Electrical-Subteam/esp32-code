idf_component_register(
    SRCS 
        "main.c" 
        "speed_calculator.c" 
        "wifi_manager.c" 
        "web_server.c"
    INCLUDE_DIRS 
        "."
    EMBED_FILES 
        "index.html"
    REQUIRES
        lvgl
    
)