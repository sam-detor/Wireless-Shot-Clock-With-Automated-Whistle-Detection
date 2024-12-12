#ifndef LED_DRIVER_H

#include <stdint.h>
void led_matrix_config(void);
void display_number(uint32_t number);
void display_one_pixel(const int pixel_num, const float brightness);
#endif