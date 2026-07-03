#ifndef WHEEL_SPEED_H
#define WHEEL_SPEED_H

#include <stdint.h>
#include "driver/gpio.h"

// ── Wheel count constant ──────────────────────────────────────────────────────
// Defined here so any file that includes this header can size arrays correctly
#define WHEEL_COUNT 4
#define MOVING_AVERAGE_WINDOW_SIZE 4

// ── Wheel identifiers ─────────────────────────────────────────────────────────
// Using an enum instead of raw integers makes call sites self-documenting.
// wheel_speed_get_last_pulse_us(WHEEL_FL) is much clearer than passing 0.
typedef enum
{
    WHEEL_FL = 0, // Front Left
    WHEEL_FR = 1, // Front Right
    WHEEL_RL = 2, // Rear Left
    WHEEL_RR = 3, // Rear Right
} wheel_id_t;

// ── Initialisation ────────────────────────────────────────────────────────────
/**
 * @brief Configure GPIO pins and attach interrupt handlers for all four
 *        Hall effect sensors. Must be called once before any other
 *        wheel_speed_* function.
 */
void hall_sensor_init(void);

// ── Data accessors ────────────────────────────────────────────────────────────
/**
 * @brief Atomically copy the current pulse counts for all four wheels into
 *        the provided array.
 *
 * @param out_counts  Array of WHEEL_COUNT uint32_t values to fill.
 *                    Index with wheel_id_t: out_counts[WHEEL_FL], etc.
 */
void wheel_speed_get_pulses(uint32_t out_counts[WHEEL_COUNT]);

/**
 * @brief Reset the pulse counter for a single wheel to zero.
 *        Useful at the start of a fixed measurement window.
 *
 * @param wheel  Which wheel to reset.
 */
void wheel_speed_reset_pulses(wheel_id_t wheel);

/**
 * @brief Return the esp_timer timestamp (microseconds since boot) of the
 *        most recent valid pulse on a given wheel.
 *
 * @param wheel  Which wheel to query.
 * @return       Timestamp in microseconds, or 0 if no pulse has been seen yet.
 */
int64_t wheel_speed_get_last_pulse_us(wheel_id_t wheel);

bool wheel_speed_get_all_rpm(float out_rpm[WHEEL_COUNT]);

#endif // WHEEL_SPEED_H