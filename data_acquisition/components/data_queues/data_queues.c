#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "../data_types/include/data_types.h"

void queues_init(void)
{
    QueueHandle_t wheelSensorQueue = xQueueCreate(32, sizeof(SensorEvent_t));

    // Crash if queue failse to initialize (avoiding untraceable bugs)
    if (wheelSensorQueue == NULL)
    {
        ESP_LOGE("Queue Creation", "Failed to create sensor event queue — insufficient heap memory perhaps?");
        abort();
    }
}