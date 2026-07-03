#ifndef WHEEL_SPEED_H
#define WHEEL_SPEED_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "data_types.h"

// ── Wheel count constant ──────────────────────────────────────────────────────
// Defined here so any file that includes this header can size arrays correctly
#define WHEEL_COUNT 4
#define MOVING_AVERAGE_WINDOW_SIZE 4

// ── Initialisation ────────────────────────────────────────────────────────────
/**
 * @brief Configure GPIO pins and attach interrupt handlers for all four
 *        Hall effect sensors. Must be called once before any other
 *        wheel_speed_* function.
 */
void hall_sensor_init(void);

// ── Data accessors ────────────────────────────────────────────────────────────
/**
 * @brief Atomically copy the current moving-average RPM for all four wheels
 *        into the provided array.
 *
 * @param out_rpm  Array of WHEEL_COUNT float values to fill.
 *                 Index with WheelIndex_t: out_rpm[WHEEL_FRONT_LEFT], etc.
 * @return         true if the read succeeded, false if the lock could not
 *                 be acquired in time (caller should keep its previous values).
 */
bool wheel_speed_get_all_rpm(float out_rpm[WHEEL_COUNT]);

#endif // WHEEL_SPEED_H