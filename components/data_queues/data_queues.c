#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "data_types.h"
#include "data_queues.h"

QueueHandle_t wheelSensorQueue;

void queues_init(void)
{
    wheelSensorQueue = xQueueCreate(32, sizeof(SensorEvent_t));

    // Crash if queue failse to initialize (avoiding untraceable bugs)
    if (wheelSensorQueue == NULL)
    {
        ESP_LOGE("Queue Creation", "Failed to create sensor event queue — insufficient heap memory perhaps?");
        abort();
    }
}