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

const uint8_t PEER_MAC_ADDRESS = 30 : C9 : 22 : 32 : CF : F4;
bool peer_connected = false;

void initialize_communication(void)
{
    esp_now_init();
    esp_now_register_recv_cb(receive_data);
    esp_now_register_send_cb(send_data);
    esp_now_add_peer(PEER_MAC_ADDRESS);
}

void receive_data(void)
{
}

void send_data(void)
{
    esp_err_t result = esp_now_send("", 8, 8);
}

void app_main(void)
{

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

    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_4, &rightfront);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_5, &leftfront);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &rightback);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_7, &leftback);

    int rightfront_value;
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
}

void reconnect_to_peer(void)
{
}
