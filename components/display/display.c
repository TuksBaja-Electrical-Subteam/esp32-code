#include "display.h"
#include "ui.h"
#include "wheel_speed.h"
#include "data_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "lvgl.h"
#include "esp_heap_caps.h"

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

// LVGL needs a chunk of RAM to render into before it's pushed to the screen.
// 1/10th of the full screen is LVGL's own recommended minimum.
#define LVGL_BUF_HEIGHT (LCD_V_RES / 10)

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_display_t *lvgl_disp = NULL;

// ── LVGL tick source ─────────────────────────────────────────────────────
// LVGL needs to know how much time has passed, in milliseconds, to drive
// animations correctly. This is called every 1ms by the esp_timer below.
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1);
}

// ── The flush callback ──────────────────────────────────────────────────
// LVGL calls this whenever it has finished rendering a chunk of pixels and
// wants them physically pushed to the display. `area` is the rectangle,
// `px_map` is the raw RGB565 pixel data. We just hand both to esp_lcd —
// all the SPI/DMA work is already handled by the panel driver you built
// earlier.
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_draw_bitmap(
        panel_handle,
        area->x1, area->y1,
        area->x2 + 1, area->y2 + 1,
        px_map);

    // Tell LVGL the flush is done so it can continue rendering.
    lv_display_flush_ready(disp);
}

// ── Telemetry → UI bridge task (unchanged from before) ──────────────────
static void display_task(void *arg)
{
    float rpm[WHEEL_COUNT];
    while (1)
    {
        if (wheel_speed_get_all_rpm(rpm))
        {
            float avg_rpm = (rpm[WHEEL_FRONT_LEFT] + rpm[WHEEL_FRONT_RIGHT] +
                              rpm[WHEEL_REAR_LEFT] + rpm[WHEEL_REAR_RIGHT]) / 4.0f;
            float speed_kmh = (avg_rpm * WHEEL_CIRCUMFERENCE_M / 60.0f) * 3.6f;
            ui_update(speed_kmh, avg_rpm);
        }

        // Let LVGL process animations, input, and trigger flushes.
        lv_timer_handler();

        vTaskDelay(pdMS_TO_TICKS(20)); // ~50Hz, plenty for a gauge UI
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

    // 1. SPI bus
    spi_bus_config_t bus_config = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    // 2. Panel IO
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
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 4. LVGL core init
    lv_init();

    // 5. LVGL tick source — a 1ms repeating timer
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 1000)); // 1000us = 1ms

    // 6. LVGL draw buffer — allocated from PSRAM/heap.
    // Two buffers = double buffering: LVGL renders into one while the
    // other is still being sent to the display.
    size_t buf_pixels = LCD_H_RES * LVGL_BUF_HEIGHT;
    lv_color_t *buf1 = heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (buf1 == NULL || buf2 == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers");
        abort();
    }

    // 7. Register the display with LVGL
    lvgl_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_flush_cb(lvgl_disp, lvgl_flush_cb);
    lv_display_set_buffers(lvgl_disp, buf1, buf2,
                           buf_pixels * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 8. Build the UI and start the telemetry task
    ui_init();
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Display task started");
}