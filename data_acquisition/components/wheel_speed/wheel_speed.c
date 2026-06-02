#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "wheel_speed.h"
#include "data_types.h"
#include "data_queues.h"

// NOTE: uS -> microseconds
#define HALL_SENSOR_DEBOUNCE_uS 500000
#define RPM_UPDATE_TIMEOUT_uS 1000000

// GPIO Pin Assignments
#define HALL_FL_GPIO GPIO_NUM_13
#define HALL_FR_GPIO GPIO_NUM_12
#define HALL_RL_GPIO GPIO_NUM_14
#define HALL_RR_GPIO GPIO_NUM_27

// The processed information about each wheel
static WheelInfo_t wheelInfos[4];

// Used in ISR Handler
static volatile int64_t last_pulse_time_us[WHEEL_COUNT] = {0};
BaseType_t higher_priority_task_woken = pdFALSE;

// Circular buffer of the MOVING_AVERAGE_WINDOW_SIZE most recent rpm values

void pushToMovingAverage(MovingAverage_t *ma, float newValue)
{
    ma->runningSum -= ma->buffer[ma->head];
    ma->buffer[ma->head] = newValue;
    ma->runningSum += newValue;
    // Advance the head. Module ensures wraps back to beginning upon reaching the end
    ma->head = (ma->head + 1) % MOVING_AVERAGE_WINDOW_SIZE;
    if (ma->count < MOVING_AVERAGE_WINDOW_SIZE)
    {
        ma->count++;
    }
}

float readMovingAverage(WheelInfo_t *wheelInfo)
{
    if (wheelInfo->movingAverage.count == 0)
    {
        return 0.0f;
    }
    int64_t now = esp_timer_get_time();
    // zero after 1 second
    if ((now - wheelInfo->lastTimestamp) > RPM_UPDATE_TIMEOUT_uS)
    {
        return 0.0f;
    };

    return wheelInfo->movingAverage.runningSum / wheelInfo->movingAverage.count;
}

void processing(void *arg)
{

    while (1)
    {
        SensorEvent_t event;

        int64_t previous = 0;

        while (xQueueReceive(wheelSensorQueue, &event, 0) == pdTRUE)
        {
            // ESP_LOGI("SENSOR", "Data received: %lu", event.sensor_id, event.timestamp_us);

            previous = wheelInfos[event.sensor_id].lastTimestamp;
            wheelInfos[event.sensor_id].lastTimestamp = event.timestamp_us;
            if (previous != 0)
            {
                float period_us = (float)(event.timestamp_us - previous);
                float period_s = period_us / 1000000.0f;
                float rpm = (1.0f / period_s) * 60.0f;

                // push rpm into moving average
                pushToMovingAverage(&wheelInfos[event.sensor_id].movingAverage, rpm);
            }
        }

        float rpm = readMovingAverage(&wheelInfos[WHEEL_REAR_RIGHT]);
        ESP_LOGI("PROCESSING", "Front Left RPM: %.2f", rpm);

        // Make task sleep for 50ms, allowing ISRs to populate queue again
        vTaskDelay(50);
    }
}

// Hall Effect Sensor Interrupt Handler
static void IRAM_ATTR hall_isr_handler(void *arg)
{
    int wheel = (int)(intptr_t)arg; // recover which wheel fired from the args
    int64_t now = esp_timer_get_time();

    // Debouncing
    if ((now - last_pulse_time_us[wheel]) > HALL_SENSOR_DEBOUNCE_uS)
    {
        last_pulse_time_us[wheel] = now;
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
