#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <stdint.h>

typedef struct
{
    uint8_t sensor_id;
    int64_t timestamp_us;
} SensorEvent_t;

#endif