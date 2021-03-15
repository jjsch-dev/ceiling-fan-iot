/*
 * MIT License
 * 
 * Copyright (c) 2021 Juan Schiavoni
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file app_main.c
 * @brief Espressif's rain-maiker based application to control a ceiling fan 
 *        without humming.
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>

#include <app_wifi.h>

#include "app_priv.h"

static const char *TAG = "app_main";

esp_rmaker_device_t *fan_device;
esp_rmaker_param_t *light_param;

esp_rmaker_device_t *thermostat_device;
esp_rmaker_param_t *thermostat_enable_param;
esp_rmaker_param_t *thermostat_slider_param;

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    char *device_name = esp_rmaker_device_get_name(device);
    char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_fan_set_power(val.val.b);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SPEED_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_fan_set_speed(val.val.i);
    } else if (strcmp(param_name, LIGHT_SWITCH_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_fan_set_ligth(val.val.b);
    } else if (strcmp(param_name, THERMOSTAT_SWITCH_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_temp_set_enable(val.val.b);
    } else if (strcmp(param_name, THERMOSTAT_SLIDER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_temp_set_level(val.val.i);
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    app_driver_init();

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_init()
     */
    app_wifi_init();
    
    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Fan");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a device and add the relevant parameters to it */
    fan_device = esp_rmaker_fan_device_create("Fan", NULL, DEFAULT_POWER);
    esp_rmaker_device_add_cb(fan_device, write_cb, NULL);
    esp_rmaker_device_add_param(fan_device, esp_rmaker_speed_param_create(ESP_RMAKER_DEF_SPEED_NAME, DEFAULT_SPEED));
    
    light_param = esp_rmaker_param_create(LIGHT_SWITCH_NAME, NULL, 
                                          esp_rmaker_bool(DEFAULT_LIGHT), 
                                          PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(light_param, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_device_add_param(fan_device, light_param);

    esp_rmaker_node_add_device(node, fan_device);

    /* Create the temperature device and add the relevant parameters to it */
    thermostat_device = esp_rmaker_temp_sensor_device_create(THERMOSTAT_DEVICE_NAME, NULL, app_get_current_temperature());
    esp_rmaker_device_add_cb(thermostat_device, write_cb, NULL);

    thermostat_enable_param = esp_rmaker_param_create(THERMOSTAT_SWITCH_NAME, NULL, 
                                                      esp_rmaker_bool(DEFAULT_THERMOSTAT_ENABLE), 
                                                      PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(thermostat_enable_param, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_device_add_param(thermostat_device, thermostat_enable_param);
    
    thermostat_slider_param = esp_rmaker_param_create(THERMOSTAT_SLIDER_NAME, NULL, 
                                                      esp_rmaker_int(DEFAULT_THERMOSTAT_TEMPERATURE), 
                                                      PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(thermostat_slider_param, ESP_RMAKER_UI_SLIDER);
    esp_rmaker_param_add_bounds(thermostat_slider_param, esp_rmaker_int(10), esp_rmaker_int(40), esp_rmaker_int(1));
    esp_rmaker_device_add_param(thermostat_device, thermostat_slider_param);

    esp_rmaker_node_add_device(node, thermostat_device);

    /* Enable scheduling.
     * Please note that you also need to set the timezone for schedules to work correctly.
     * Simplest option is to use the CONFIG_ESP_RMAKER_DEF_TIMEZONE config option.
     * Else, you can set the timezone using the API call `esp_rmaker_time_set_timezone("Asia/Shanghai");`
     * For more information on the various ways of setting timezone, please check
     * https://rainmaker.espressif.com/docs/time-service.html.
     */
    esp_rmaker_schedule_enable();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}
