#include "display.h"
#include "ui.h"
#include "wheel_speed.h"
#include "data_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "DISPLAY"
#define WHEEL_CIRCUMFERENCE_M 1.72f

static void display_task(void *arg)
{
    float rpm[WHEEL_COUNT];

    while (1)
    {
        if (wheel_speed_get_all_rpm(rpm))
        {
            float rr_rpm = rpm[WHEEL_REAR_RIGHT];
            float speed_kmh = (rr_rpm * WHEEL_CIRCUMFERENCE_M / 60.0f) * 3.6f;
            ui_update(speed_kmh, rr_rpm);
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz
    }
}

void display_init(void)
{
    ui_init();
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Display task started");
}