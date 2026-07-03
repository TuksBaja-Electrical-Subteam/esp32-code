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
 * @brief Get the current moving-average RPM for all four wheels.
 *
 * Thread-safe: acquires wheelInfoMutex internally, so this is safe to call
 * from any task (display, logging, etc.) without additional locking.
 *
 * USAGE:
 *   1. Declare a local array: float rpm[WHEEL_COUNT];
 *   2. Call this function, passing that array.
 *   3. Check the return value before using the data — see below.
 *   4. Index into the array using WheelIndex_t (data_types.h), e.g.
 *      rpm[WHEEL_FRONT_LEFT], rpm[WHEEL_REAR_RIGHT].
 *
 * Example:
 *   float rpm[WHEEL_COUNT];
 *   if (wheel_speed_get_all_rpm(rpm))
 *   {
 *       // rpm[] now holds fresh values, safe to read/display/log
 *   }
 *
 * Call this periodically from within your own task's loop (e.g. once per
 * display refresh, every 100-200ms) — it does not block waiting for new
 * sensor data, it just returns whatever the current moving average is at
 * the moment of the call.
 *
 * @param out_rpm  Caller-provided array of WHEEL_COUNT floats to fill.
 * @return true  if the read succeeded and out_rpm now holds fresh values.
 * @return false if the mutex could not be acquired within the timeout
 *               (rare — indicates heavy lock contention). In this case
 *               out_rpm is left untouched; the caller should keep using
 *               its previous values rather than treating this as zero.
 */
bool wheel_speed_get_all_rpm(float out_rpm[WHEEL_COUNT]);

#endif // WHEEL_SPEED_H