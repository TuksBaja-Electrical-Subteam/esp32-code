#include "driver/spi_common.h" // spi_bus_initialize, spi_bus_config_t
#include "driver/gpio.h"       // gpio_set_level, gpio_set_direction
#include "esp_lcd_panel_io.h"  // esp_lcd_new_panel_io_spi
#include "esp_lcd_panel_ops.h" // esp_lcd_panel_reset, esp_lcd_panel_init
#include "esp_lvgl_port.h"     // lvgl_port_init, lvgl_port_add_disp
#include "lvgl.h"              // lv_label_create, lv_scr_act etc
#include "rm68140.h"           // your custom panel driver

// --- Pin config — adjust to your board ---
#define LCD_PIN_MOSI 23
#define LCD_PIN_CLK 18
#define LCD_PIN_CS 5
#define LCD_PIN_DC 2
#define LCD_PIN_RST 4
#define LCD_PIN_BL 15

#define LCD_H_RES 320
#define LCD_V_RES 480
#define LCD_SPI_CLOCK (40 * 1000 * 1000) // 40 MHz

#include <stdio.h>
#include "esp_now.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
static const char *TAG = "data_acquisition";

/* const uint8_t PEER_MAC_ADDRESS = 30 : C9 : 22 : 32 : CF : F4;
bool peer_connected = false;

void initialize_communication(void)
{
    esp_now_init();
    esp_now_register_recv_cb(receive_data);
    esp_now_register_send_cb(send_data);
    // esp_now_add_peer(PEER_MAC_ADDRESS);
}

void receive_data(void)
{
}

void send_data(void)
{
    esp_err_t result = esp_now_send("", 8, 8);
}
*/

void app_main(void)
{

    // 1. Backlight on
    gpio_set_direction(LCD_PIN_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_PIN_BL, 1);

    // 2. SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 20 * sizeof(uint16_t), // 20-line DMA buffer
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. Panel IO (SPI → LCD)
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_SPI_CLOCK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle));

    // 4. RM68140 panel driver
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(rm68140_new_panel(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 5. LVGL port init (creates task + timer internally)
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // 6. Register display with LVGL port
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * 20, // 20-line buffer
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true, // DMA-backed buffer
            .swap_bytes = false,
        },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_LOGI(TAG, "Display registered: %p", disp);

    // 7. Draw something — always lock before calling LVGL APIs
    if (lvgl_port_lock(0))
    {
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "RM68140 + LVGL v9");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lvgl_port_unlock();
    }

    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,

    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t rightfront = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    adc_oneshot_chan_cfg_t leftfront = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_chan_cfg_t rightback = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_chan_cfg_t leftback = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    }; 



    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_4, &rightfront); //GPIO32
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_5, &leftfront);//GPIO33
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &rightback);//GPIO34
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_7, &leftback);//GPIO35

    /*int rightfront_value;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_4, &rightfront_value);

    int leftfront_value;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_5, &leftfront_value);

    int rightback_value;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &rightback_value);

    int leftback_value;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &leftback_value);

    ESP_LOGI(TAG, "%d", rightfront_value);
    ESP_LOGI(TAG, "%d", leftfront_value);
    ESP_LOGI(TAG, "%d", rightback_value);
    ESP_LOGI(TAG, "%d", leftback_value);

    */
    
     while (1) {
        int values[4];
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_4, &values[0]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_5, &values[1]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &values[2]));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &values[3]));

        // 🔍 Sift through data: find max value safely
        int max_val = values[0];
        int max_idx = 0;
        for (int i = 1; i < 4; i++) {
            if (values[i] > max_val) {
                max_val = values[i];
                max_idx = i;
            }
        }

        ESP_LOGI(TAG, "RF:%d LF:%d RB:%d LB:%d | Max: CH%d=%d",
                 values[0], values[1], values[2], values[3],
                 max_idx, max_val);

        // 🛡 Stability: add delay to avoid CPU hogging
        vTaskDelay(pdMS_TO_TICKS(200));
    }

}

void reconnect_to_peer(void)
{
}
