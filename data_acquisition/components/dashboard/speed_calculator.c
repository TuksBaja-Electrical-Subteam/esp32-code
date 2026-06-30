#include "speed_calculator.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SPEED_CALC";
static pcnt_unit_handle_t pcnt_unit = NULL;
static float _current_speed = 0.0;
static float _total_distance_km = 0.0;
static float _wheel_circumference = 1.72;

float get_current_speed(void) { return _current_speed; }
float get_total_distance(void) { return _total_distance_km; }
void reset_odometer(void) { _total_distance_km = 0.0; }

static void speed_calc_task(void* pvParameters) {
    int pulses = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (pcnt_unit) {
            ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulses));
            ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));

            // Accumulate distance: (pulses * circumference) / 1000 to get km
            _total_distance_km += (pulses * _wheel_circumference) / 1000.0;

            // Calculate speed: pulses per sec * circumference * 3.6 for km/h
            _current_speed = (pulses * _wheel_circumference) * 3.6;
            
            ESP_LOGI(TAG, "Speed: %.1f km/h | Odo: %.3f km", _current_speed, _total_distance_km);
        }
    }
}

void speed_calc_init(int gpio_num, float initial_circumference) {
    if (pcnt_unit != NULL) return;
    _wheel_circumference = initial_circumference;
    
    gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);

    pcnt_unit_config_t unit_config = { .high_limit = 2000, .low_limit = -2000 }; 
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_chan_config_t chan_config = { .edge_gpio_num = gpio_num, .level_gpio_num = -1 };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));

    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 10000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    xTaskCreate(speed_calc_task, "speed_calc", 4096, NULL, 5, NULL);
}