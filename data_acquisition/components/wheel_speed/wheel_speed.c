#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "wheel_speed.h"
#include "../data_types/include/data_types.h"
#include "../data_queues/include/data_queues.h"

// GPIO Pin Assignments
#define HALL_FL_GPIO GPIO_NUM_13
#define HALL_FR_GPIO GPIO_NUM_12
#define HALL_RL_GPIO GPIO_NUM_14
#define HALL_RR_GPIO GPIO_NUM_27

static volatile int64_t last_pulse_time_us[WHEEL_COUNT] = {0};
BaseType_t higher_priority_task_woken = pdFALSE;

static void processing(void *arg)
{
    SensorEvent_t event;
    while (1)
    {
        if (xQueueReceive(wheelSensorQueue, &event, portMAX_DELAY))
        {
            ESP_LOGI("SENSOR", "Data received: %lu", event.sensor_id, event.timestamp_us);
        }
    }
}

// Hall Effect Sensor Interrupt Handler
static void IRAM_ATTR hall_isr_handler(void *arg)
{
    int wheel = (int)(intptr_t)arg; // recover which wheel fired from the args
    int64_t now = esp_timer_get_time();

    // Debouncing (500ms)
    if ((now - last_pulse_time_us[wheel]) > 500000)
    {

        SensorEvent_t sensorEvent = {
            .sensor_id = wheel,
            .timestamp_us = now,
        };

        // Push Sensor Event to queue
        xQueueSendFromISR(wheelSensorQueue, &sensorEvent, &higher_priority_task_woken);
    }
}

// Called once in main.c
void hall_sensor_init(void)
{
    gpio_install_isr_service(0);

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
    gpio_isr_handler_add(HALL_FL_GPIO, hall_isr_handler, (void *)(intptr_t)0);
    gpio_isr_handler_add(HALL_FR_GPIO, hall_isr_handler, (void *)(intptr_t)1);
    gpio_isr_handler_add(HALL_RL_GPIO, hall_isr_handler, (void *)(intptr_t)2);
    gpio_isr_handler_add(HALL_RR_GPIO, hall_isr_handler, (void *)(intptr_t)3);

    xTaskCreate(processing, "processing", 2048, NULL, 10, NULL);
}
