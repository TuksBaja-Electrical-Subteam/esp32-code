#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "rm68140.h"

static const char *TAG = "rm68140";

typedef struct
{
    esp_lcd_panel_t base; // MUST be first
    esp_lcd_panel_io_handle_t io;
    int reset_gpio;
    int x_gap;
    int y_gap;
    uint8_t madctl; // current orientation bits
} rm68140_panel_t;

// Forward declarations
static esp_err_t panel_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_init(esp_lcd_panel_t *panel);
static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                   int x_end, int y_end, const void *color_data);
static esp_err_t panel_invert_color(esp_lcd_panel_t *panel, bool invert);
static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool x, bool y);
static esp_err_t panel_swap_xy(esp_lcd_panel_t *panel, bool swap);
static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_disp_on_off(esp_lcd_panel_t *panel, bool on_off);
static esp_err_t panel_del(esp_lcd_panel_t *panel);

esp_err_t rm68140_new_panel(const esp_lcd_panel_io_handle_t io,
                            const esp_lcd_panel_dev_config_t *cfg,
                            esp_lcd_panel_handle_t *ret_panel)
{
    rm68140_panel_t *rm = calloc(1, sizeof(rm68140_panel_t));
    ESP_RETURN_ON_FALSE(rm, ESP_ERR_NO_MEM, TAG, "no mem");

    rm->io = io;
    rm->reset_gpio = cfg->reset_gpio_num;
    rm->madctl = 0x00;

    rm->base.reset = panel_reset;
    rm->base.init = panel_init;
    rm->base.draw_bitmap = panel_draw_bitmap;
    rm->base.invert_color = panel_invert_color;
    rm->base.mirror = panel_mirror;
    rm->base.swap_xy = panel_swap_xy;
    rm->base.set_gap = panel_set_gap;
    rm->base.disp_on_off = panel_disp_on_off;
    rm->base.del = panel_del;

    if (rm->reset_gpio >= 0)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = BIT64(rm->reset_gpio),
            .mode = GPIO_MODE_OUTPUT,
        };
        gpio_config(&io_conf);
    }

    *ret_panel = &rm->base;
    return ESP_OK;
}

static esp_err_t panel_reset(esp_lcd_panel_t *panel)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    if (rm->reset_gpio >= 0)
    {
        gpio_set_level(rm->reset_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(rm->reset_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    else
    {
        // Software reset
        esp_lcd_panel_io_tx_param(rm->io, 0x01, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_t *panel)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    esp_lcd_panel_io_handle_t io = rm->io;

    // --- RM68140 init sequence ---
    // Adjust these per your panel manufacturer's datasheet
    esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0); // Sleep out
    vTaskDelay(pdMS_TO_TICKS(120));

    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]){0x00}, 1); // MADCTL
    esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]){0x55}, 1); // RGB565

    // Power settings (check your panel's init sheet for exact values)
    esp_lcd_panel_io_tx_param(io, 0xD0, (uint8_t[]){0x07, 0x42, 0x18}, 3);
    esp_lcd_panel_io_tx_param(io, 0xD1, (uint8_t[]){0x00, 0x07, 0x10}, 3);
    esp_lcd_panel_io_tx_param(io, 0xD2, (uint8_t[]){0x01, 0x02}, 2);

    // Gamma
    esp_lcd_panel_io_tx_param(io, 0xC8,
                              (uint8_t[]){0x00, 0x32, 0x36, 0x45, 0x06, 0x16, 0x37, 0x75, 0x77, 0x54, 0x0C, 0x00}, 12);

    esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0); // Display on
    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel,
                                   int x_start, int y_start,
                                   int x_end, int y_end,
                                   const void *color_data)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    x_start += rm->x_gap;
    x_end += rm->x_gap;
    y_start += rm->y_gap;
    y_end += rm->y_gap;

    // Column address set (CASET)
    esp_lcd_panel_io_tx_param(rm->io, 0x2A, (uint8_t[]){
                                                (x_start >> 8) & 0xFF,
                                                x_start & 0xFF,
                                                ((x_end - 1) >> 8) & 0xFF,
                                                (x_end - 1) & 0xFF,
                                            },
                              4);

    // Row address set (RASET)
    esp_lcd_panel_io_tx_param(rm->io, 0x2B, (uint8_t[]){
                                                (y_start >> 8) & 0xFF,
                                                y_start & 0xFF,
                                                ((y_end - 1) >> 8) & 0xFF,
                                                (y_end - 1) & 0xFF,
                                            },
                              4);

    // Memory write (RAMWR)
    size_t pixel_count = (x_end - x_start) * (y_end - y_start);
    esp_lcd_panel_io_tx_color(rm->io, 0x2C, color_data, pixel_count * 2);

    return ESP_OK;
}

static esp_err_t panel_invert_color(esp_lcd_panel_t *panel, bool invert)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    esp_lcd_panel_io_tx_param(rm->io, invert ? 0x21 : 0x20, NULL, 0);
    return ESP_OK;
}

static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool x, bool y)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    if (x)
        rm->madctl |= BIT(6);
    else
        rm->madctl &= ~BIT(6);
    if (y)
        rm->madctl |= BIT(7);
    else
        rm->madctl &= ~BIT(7);
    esp_lcd_panel_io_tx_param(rm->io, 0x36, &rm->madctl, 1);
    return ESP_OK;
}

static esp_err_t panel_swap_xy(esp_lcd_panel_t *panel, bool swap)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    if (swap)
        rm->madctl |= BIT(5);
    else
        rm->madctl &= ~BIT(5);
    esp_lcd_panel_io_tx_param(rm->io, 0x36, &rm->madctl, 1);
    return ESP_OK;
}

static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    rm->x_gap = x_gap;
    rm->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_disp_on_off(esp_lcd_panel_t *panel, bool on)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    esp_lcd_panel_io_tx_param(rm->io, on ? 0x29 : 0x28, NULL, 0);
    return ESP_OK;
}

static esp_err_t panel_del(esp_lcd_panel_t *panel)
{
    rm68140_panel_t *rm = __containerof(panel, rm68140_panel_t, base);
    if (rm->reset_gpio >= 0)
        gpio_reset_pin(rm->reset_gpio);
    free(rm);
    return ESP_OK;
}