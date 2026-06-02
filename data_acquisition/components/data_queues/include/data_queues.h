#ifndef DATA_QUEUES_H
#define DATA_QUEUES_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t wheelSensorQueue;
void queues_init(void);

#endif