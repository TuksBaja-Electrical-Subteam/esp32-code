#include <stdio.h>
#include "data_types.h"
#include "data_queues.h"
#include "wheel_speed.h"

void app_main(void)
{
    queues_init();
    hall_sensor_init();
}
