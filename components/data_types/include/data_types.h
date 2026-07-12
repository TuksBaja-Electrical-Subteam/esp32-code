#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <stdint.h>

#define MOVING_AVERAGE_WINDOW_SIZE 10
#define WHEEL_COUNT 4

typedef enum
{
    WHEEL_FRONT_LEFT = 0,
    WHEEL_FRONT_RIGHT = 1,
    WHEEL_REAR_LEFT = 2,
    WHEEL_REAR_RIGHT = 3,
} WheelIndex_t;

typedef struct
{
    uint8_t sensor_id;
    int64_t timestamp_us;
} SensorEvent_t;

typedef struct
{
    int64_t head;
    float buffer[MOVING_AVERAGE_WINDOW_SIZE];
    float runningSum;
    int16_t count;

} MovingAverage_t;

typedef struct
{
    MovingAverage_t movingAverage;
    int64_t lastTimestamp;

} WheelInfo_t;

#endif