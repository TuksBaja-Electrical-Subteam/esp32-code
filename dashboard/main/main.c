#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "speed_calculator.h"
#include "web_server.h"
#include "gps_handler.h"

#define TAG "MAIN"
#define HALL_SENSOR_GPIO 18
#define WHEEL_CIRCUMFERENCE 1.72f
#define GPS_TX_GPIO 17
#define GPS_RX_GPIO 16

void app_main(void) {
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "   BAJA Dash PRO - Starting");
    ESP_LOGI(TAG, "=================================");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize sensors
    speed_calc_init(HALL_SENSOR_GPIO, WHEEL_CIRCUMFERENCE);
    gps_init(GPS_TX_GPIO, GPS_RX_GPIO);
    
    // Initialize WiFi AP
    wifi_init_softap();
    
    // Start web server
    start_webserver();
    
    ESP_LOGI(TAG, "Initialization complete. Dashboard: http://192.168.4.1");
    
    // Main loop with periodic logging
    uint32_t last_log = 0;
    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (now - last_log >= 5000) {
            ESP_LOGI(TAG, "Speed=%.1f km/h, Dist=%.3f km, GPS=[%.6f, %.6f]",
                     (double)get_current_speed(),
                     (double)get_total_distance(),
                     (double)get_gps_latitude(),
                     (double)get_gps_longitude());
            last_log = now;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}