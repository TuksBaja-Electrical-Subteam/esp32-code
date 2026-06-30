#ifndef SPEED_CALCULATOR_H
#define SPEED_CALCULATOR_H

void speed_calc_init(int gpio_num, float initial_circumference);
float get_current_speed(void);
float get_total_distance(void);
void reset_odometer(void);

#endif