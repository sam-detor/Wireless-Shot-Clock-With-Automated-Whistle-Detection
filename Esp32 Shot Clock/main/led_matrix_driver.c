/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "led_driver.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      12//13

#define EXAMPLE_LED_NUMBERS         (32 * 32)
#define EXAMPLE_CHASE_SPEED_MS      10

static const char *TAG = "example";

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

const uint32_t rows_zero[52] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,24,24,24,24,24,24,24,24};
const uint32_t columns_zero[52] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};

const uint32_t rows_one[36] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24};
const uint32_t columns_one[36] = {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};

const uint32_t rows_two[43] = {7,8,9,10,11,12,13,14,15,16,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,24,24,24,24,24,24,24,24,16,16,16,16,16,16,16,16};
const uint32_t columns_two[43] = {0,0,0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};

const uint32_t rows_three[42] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,24,24,24,24,24,24,24,24,16,16,16,16,16,16,16,16};
const uint32_t columns_three[42] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};

const uint32_t rows_four[36] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,16,16,16,16,16,16,16,16,7,8,9,10,11,12,13,14,15,16};
const uint32_t columns_four[36] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,9,9,9,9,9,9,9,9,9};

const uint32_t rows_five[44] = {7,8,9,10,11,12,13,14,15,16,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,24,24,24,24,24,24,24,24,16,16,16,16,16,16,16,16,24};
const uint32_t columns_five[44] = {9,9,9,9,9,9,9,9,9,9,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,9};

const uint32_t rows_six[51] = {16,17,18,19,20,21,22,23,24,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,24,24,24,24,24,24,24,24,16,16,16,16,16,16,16,16};
const uint32_t columns_six[51] = {0,0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};

const uint32_t rows_seven[27] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,7};
const uint32_t columns_seven[27] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9};

const uint32_t rows_eight[60] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,7,7,7,7,7,7,7,7,24,24,24,24,24,24,24,24,16,16,16,16,16,16,16,16};
const uint32_t columns_eight[60] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8,1,2,3,4,5,6,7,8};

const uint32_t rows_nine[44] = {7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,16,16,16,16,16,16,16,16,7,8,9,10,11,12,13,14,15,16,7,7,7,7,7,7,7,7};
const uint32_t columns_nine[44] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,9,9,9,9,9,9,9,9,9,1,2,3,4,5,6,7,8};

const uint32_t* rows_digits[10] = {rows_zero,rows_one,rows_two,rows_three,rows_four,rows_five,rows_six,rows_seven,rows_eight,rows_nine};
const uint32_t* columns_digits[10] = {columns_zero,columns_one,columns_two,columns_three,columns_four,columns_five,columns_six,columns_seven,columns_eight,columns_nine};
const uint32_t lengths[10] = {52,36,43,42,36,44,51,27,60,44};

rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;
rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, const uint32_t s, const uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void write_pixel(const int pixel_num, const float brightness)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    led_strip_hsv2rgb(0, 0, 100, &red, &green, &blue);
    led_strip_pixels[pixel_num * 3 + 0] = green * brightness;
    led_strip_pixels[pixel_num * 3 + 1] = blue * brightness;
    led_strip_pixels[pixel_num * 3 + 2] = red * brightness;
}

uint32_t coordinate_to_serpintine_address(const uint32_t row, const uint32_t column)
{
   if (row < 8)
   {
        if (column % 2 == 0)
        {
            return 768 + (column * 8) + (7 - row);
        }
        
        return 768 + (column * 8) + row;

   }
   else if (row < 16)
   {
        uint32_t relative_row = row - 8;
        if (column % 2 == 0)
        {
            return 512 + ((31 - column) * 8) + (7 - relative_row);
        }
        
        return 512 + ((31 - column) * 8) + relative_row;
   }
   else if (row < 24)
   {
        uint32_t relative_row = row - 16;
        if (column % 2 == 0)
        {
            return 256 + (column * 8) + (7 - relative_row);
        }
        
        return 256 + (column * 8) + relative_row;
   }
   else //row between 24 and 31
   {
        uint32_t relative_row = row - 24;
        if (column % 2 == 0)
        {
            return ((31 - column) * 8) + (7 - relative_row);
        }
        
        return ((31 - column) * 8) + relative_row;

   }
}

void write_digit(const uint32_t* rows,const uint32_t* columns, const uint32_t length, const uint32_t shift)
{
    for (int i = 0; i < length; i++)
    {
        uint32_t pixel_num = coordinate_to_serpintine_address(rows[i], columns[i] + shift);
        write_pixel(pixel_num, 0.05);
    }
}

/*void display_one_pixel(const int pixel_num, const float brightness)
{
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));

    //write_digit(rows_nine_n,columns_nine_n, 44, 4); //right digit shift

    //write_digit(rows_nine_n,columns_nine_n, 44, 18); //left digit shift

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}*/

void led_matrix_config(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 512, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));
}

void display_number(uint32_t number)
{
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
       
        if (number < 10)
        {
            write_digit(rows_digits[number], columns_digits[number], lengths[number], 11);
        }
        else
        {
            write_digit(rows_digits[number / 10], columns_digits[number / 10], lengths[number / 10], 18);
            write_digit(rows_digits[number % 10], columns_digits[number % 10], lengths[number % 10], 4);
        }

        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}
