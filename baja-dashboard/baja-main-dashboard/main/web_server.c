#include <esp_http_server.h>
#include "esp_log.h"
#include "web_server.h"
#include "speed_calculator.h"
#include "gps_handler.h"

#define TAG "WEB_SERVER"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, 
                    index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t data_get_handler(httpd_req_t *req) {
    char val[64];
    snprintf(val, sizeof(val), "%.1f,%.3f,%.6f,%.6f", 
             (double)get_current_speed(), 
             (double)get_total_distance(),
             (double)get_gps_latitude(),
             (double)get_gps_longitude());
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, val, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t sessions_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "[]", 2);
    return ESP_OK;
}

static esp_err_t track_map_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"type\":\"LineString\",\"coordinates\":[]}", 
                    HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.lru_purge_enable = true;
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
        
        httpd_uri_t root_uri = { 
            .uri = "/", 
            .method = HTTP_GET, 
            .handler = root_get_handler 
        };
        
        httpd_uri_t data_uri = { 
            .uri = "/data", 
            .method = HTTP_GET, 
            .handler = data_get_handler 
        };
        
        httpd_uri_t sessions_uri = { 
            .uri = "/sessions", 
            .method = HTTP_GET, 
            .handler = sessions_get_handler 
        };
        
        httpd_uri_t track_uri = { 
            .uri = "/track/map", 
            .method = HTTP_GET, 
            .handler = track_map_get_handler 
        };
        
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &data_uri);
        httpd_register_uri_handler(server, &sessions_uri);
        httpd_register_uri_handler(server, &track_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}