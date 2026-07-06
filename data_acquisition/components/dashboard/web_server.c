#include <string.h>
#include <esp_http_server.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "web_server.h"
#include "speed_calculator.h"
#include "gps_handler.h"
#include "wheel_speed.h"
#include "data_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "WEB_SERVER"
#define TELEMETRY_PERIOD_MS 100
#define WHEEL_CIRCUMFERENCE_M 1.72f // mirrors display.c's WHEEL_CIRCUMFERENCE_M
#define WS_MAX_CLIENTS 7 // must match httpd_config_t.max_open_sockets in start_webserver()

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server = NULL;

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

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // Initial handshake — nothing else to do, telemetry is server-push only.
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    // Discard any client-sent frame; the dashboard never calls ws.send().
    return httpd_ws_recv_frame(req, &ws_pkt, 0);
}

void telemetry_broadcast(const telemetry_frame_t* frame) {
    if (s_server == NULL) {
        return;
    }

    char json[64];
    int len = snprintf(json, sizeof(json), "{\"speed_ms\":%.3f}", (double)frame->speed_ms);
    if (len <= 0 || (size_t)len >= sizeof(json)) {
        return;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t *)json;
    ws_pkt.len = (size_t)len;

    static int client_fds[WS_MAX_CLIENTS];
    size_t fd_count = WS_MAX_CLIENTS;
    if (httpd_get_client_list(s_server, &fd_count, client_fds) != ESP_OK) {
        return;
    }

    for (size_t i = 0; i < fd_count; i++) {
        int sockfd = client_fds[i];
        if (httpd_ws_get_fd_info(s_server, sockfd) == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(s_server, sockfd, &ws_pkt);
        }
    }
}

static void telemetry_task(void *arg) {
    float rpm[WHEEL_COUNT] = {0};
    while (1) {
        // On lock contention wheel_speed_get_all_rpm() returns false and leaves
        // rpm[] untouched, so we keep broadcasting the last-known-good values.
        wheel_speed_get_all_rpm(rpm);

        float avg_rpm = (rpm[WHEEL_FRONT_LEFT] + rpm[WHEEL_FRONT_RIGHT] +
                          rpm[WHEEL_REAR_LEFT] + rpm[WHEEL_REAR_RIGHT]) / 4.0f;
        float speed_ms = (avg_rpm * WHEEL_CIRCUMFERENCE_M) / 60.0f;

        telemetry_frame_t frame = {0};
        frame.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        frame.speed_ms = speed_ms;
        telemetry_broadcast(&frame);

        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }
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

        httpd_uri_t ws_uri = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws_handler,
            .is_websocket = true
        };

        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &data_uri);
        httpd_register_uri_handler(server, &sessions_uri);
        httpd_register_uri_handler(server, &track_uri);
        httpd_register_uri_handler(server, &ws_uri);

        s_server = server;
        xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);

        ESP_LOGI(TAG, "Web server started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}