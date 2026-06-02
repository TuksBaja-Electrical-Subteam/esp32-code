#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"

// GPIO CONFIG
#define HALL_FL_GPIO GPIO_NUM_4
#define HALL_FR_GPIO GPIO_NUM_4
#define HALL_RL_GPIO GPIO_NUM_4
#define HALL_RR_GPIO GPIO_NUM_4

// Hall Sensor Interrupt
static volatile uint32_t pulse_count = 0;
static volatile int64_t last_pulse_time_us = 0;

static void IRAM_ATTR hall_isr_handler(void *arg)
{
    // Debouncing
    int64_t now = esp_timer_get_time();
    if ((now - last_pulse_time_us) > 5000)
    {
        pulse_count++;
        last_pulse_time_us = now;
    }
}

void hall_sensor_init(void)
{
    gpio_config_t HALL_FL_CONF = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << HALL_FL_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config_t HALL_FR_CONF = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << HALL_FR_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config_t HALL_RL_CONF = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << HALL_RL_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config_t HALL_RR_CONF = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << HALL_RR_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    gpio_config(&HALL_FL_CONF);
    gpio_config(&HALL_FR_CONF);
    gpio_config(&HALL_RL_CONF);
    gpio_config(&HALL_RR_CONF);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(HALL_FL_GPIO, hall_isr_handler, NULL);
    gpio_isr_handler_add(HALL_FR_GPIO, hall_isr_handler, NULL);
    gpio_isr_handler_add(HALL_RL_GPIO, hall_isr_handler, NULL);
    gpio_isr_handler_add(HALL_RR_GPIO, hall_isr_handler, NULL);
}
