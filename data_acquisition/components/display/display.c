#include "display.h"
#include "ui.h"
#include "wheel_speed.h"
#include "data_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"

#define LCD_HOST SPI2_HOST
#define PIN_LCD_SCLK GPIO_NUM_18
#define PIN_LCD_MOSI GPIO_NUM_23
#define PIN_LCD_CS GPIO_NUM_5
#define PIN_LCD_DC GPIO_NUM_2
#define PIN_LCD_RST GPIO_NUM_4
#define PIN_LCD_BL GPIO_NUM_15
#define LCD_H_RES 240
#define LCD_V_RES 320
#define TAG "DISPLAY"
#define WHEEL_CIRCUMFERENCE_M 1.72f

static void display_task(void *arg)
{
    float rpm[WHEEL_COUNT];
    while (1)
    {
        if (wheel_speed_get_all_rpm(rpm))
        {
            float rr_rpm = rpm[WHEEL_REAR_RIGHT];
            float speed_kmh = (rr_rpm * WHEEL_CIRCUMFERENCE_M / 60.0f) * 3.6f;
            ui_update(speed_kmh, rr_rpm);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz
    }
}

void display_init(void)
{
    // Backlight
    gpio_config_t bl_config = {
        .pin_bit_mask = (1ULL << PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_config);
    gpio_set_level(PIN_LCD_BL, 1);

    // 1. SPI bus (physical wires, shared)
    spi_bus_config_t bus_config = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    // 2. Panel IO (handles CS/DC pin timing for you)
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 3. ILI9341 panel driver
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 4. LVGL + telemetry task (flush callback not wired yet — next step)
    ui_init();
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Display task started");
}