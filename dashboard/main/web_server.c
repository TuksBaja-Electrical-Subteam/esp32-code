#include <esp_http_server.h>
#include "web_server.h"
#include "speed_calculator.h"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t data_get_handler(httpd_req_t *req)
{
    char val[32];
    // Formats as "speed,distance"
    snprintf(val, sizeof(val), "%.1f,%.3f", get_current_speed(), get_total_distance());
    httpd_resp_send(req, val, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
        httpd_uri_t data_uri = {.uri = "/data", .method = HTTP_GET, .handler = data_get_handler};
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &data_uri);
    }
}