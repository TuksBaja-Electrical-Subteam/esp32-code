#include "gps_handler.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "GPS"
#define UART_NUM UART_NUM_1
#define BUF_SIZE 1024

static float _lat = 0.0f;
static float _lon = 0.0f;

// GPS reader task (proper C function, NOT lambda)
static void gps_reader_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, 
                                   100 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGD(TAG, "GPS RX: %.*s", len, data);
            
            // Basic NMEA parsing - look for GPRMC
            char *p = strstr((char *)data, "$GPRMC");
            if (p) {
                // Simple parse for lat/lon
                char *token = strtok(p, ",");
                if (token) token = strtok(NULL, ",");  // Skip $GPRMC
                if (token) token = strtok(NULL, ",");  // Skip time
                if (token && token[0] == 'A') {  // Active fix
                    token = strtok(NULL, ",");  // Latitude
                    if (token) {
                        float lat_val = atof(token) / 100.0f;
                        char *dot = strchr(token, '.');
                        if (dot) {
                            float minutes = atof(dot);
                            lat_val = (float)((int)(atof(token) / 100.0f)) + (minutes / 60.0f);
                        }
                        token = strtok(NULL, ",");  // N/S
                        if (token && token[0] == 'S') lat_val = -lat_val;
                        _lat = lat_val;
                    }
                    
                    token = strtok(NULL, ",");  // Longitude
                    if (token) {
                        float lon_val = atof(token) / 100.0f;
                        char *dot = strchr(token, '.');
                        if (dot) {
                            float minutes = atof(dot);
                            lon_val = (float)((int)(atof(token) / 100.0f)) + (minutes / 60.0f);
                        }
                        token = strtok(NULL, ",");  // E/W
                        if (token && token[0] == 'W') lon_val = -lon_val;
                        _lon = lon_val;
                    }
                    
                    ESP_LOGD(TAG, "GPS Fix: %.6f, %.6f", (double)_lat, (double)_lon);
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void gps_init(int tx_io, int rx_io) {
    ESP_LOGI(TAG, "Initializing GPS on UART1 (TX:%d, RX:%d)", tx_io, rx_io);
    
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, tx_io, rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    
    // Create GPS reader task (proper C function)
    xTaskCreate(gps_reader_task, "gps_reader", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "GPS initialization complete");
}

float get_gps_latitude(void) {
    return _lat;
}

float get_gps_longitude(void) {
    return _lon;
}