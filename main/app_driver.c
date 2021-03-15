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
 * @file app_driver.c
 * @brief Based on the Espressif example, it implements the low-level drivers 
 *        that control the fan (relays / thermistor, led).
 */

#include <sdkconfig.h>

#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#include "app_priv.h"

#include "rotary_encoder.h"
#include "thermistor.h"
#include "math.h"

#include "esp_log.h"
static const char* TAG = "app_drv";

#ifdef CONFIG_IDF_TARGET_ESP32C3
    #include <ws2812_led.h>
#endif

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_ROT_ENC_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static uint8_t g_speed = DEFAULT_SPEED;
static bool g_power = DEFAULT_POWER;
static bool g_light = DEFAULT_LIGHT;
static float g_temperature = 0;
static bool g_temp_enable = DEFAULT_THERMOSTAT_ENABLE;
static int g_temp_level = DEFAULT_THERMOSTAT_TEMPERATURE;

// Create rotary encoder instance, and timer
static esp_timer_handle_t temperature_timer;
static rotenc_handle_t h_encoder = { 0 };

static thermistor_handle_t th;
static int32_t last_encoder_position = 0; 

// Converts the choice of menuconfig into the enums of the ADC channels.
#if CONFIG_ADC_CHANNEL_1
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_1
#elif CONFIG_ADC_CHANNEL_2
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_2
#elif CONFIG_ADC_CHANNEL_3
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_3
#elif CONFIG_ADC_CHANNEL_4
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_4
#elif CONFIG_ADC_CHANNEL_5
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_5
#elif CONFIG_ADC_CHANNEL_6
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_6
#elif CONFIG_ADC_CHANNEL_7
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_7
#elif CONFIG_ADC_CHANNEL_8
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_8
#elif CONFIG_ADC_CHANNEL_9
	#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_9
#else
    #error "Configure the ADC channel where the thermistor is connected"
#endif

/**
 * @brief Initialize the LED driver, with the ESP32-C3-Devkitm a neopixel 
 *        is used.
 */
static esp_err_t init_led(void)
{
esp_err_t err = ESP_OK;

#ifdef CONFIG_IDF_TARGET_ESP32C3
    err = ws2812_led_init();
    if (err == ESP_OK) {
        err = ws2812_led_clear();
    }
    ESP_LOGI(TAG, "ws2812_led_init: %d", err);
#endif
    return err;
}

/**
 * @brief Re-maps a number from one range to another. That is, a value of fromLow 
 *        would get mapped to toLow, a value of fromHigh to toHigh, values in-between 
 *        to values in-between, etc.
 *        NOTE: this function was inspired by Arduino: 
 *        https://www.arduino.cc/reference/en/language/functions/math/map/
 * 
 * @param x The number to map.
 * @param in_min The lower bound of the value’s current range.
 * @param in_max The upper bound of the value’s current range.
 * @param out_min The lower bound of the value’s target range.
 * @param out_max The upper bound of the value’s target range
 * @return The mapped value.
.*/
static uint32_t num_map(uint32_t x, 
                        uint32_t in_min, uint32_t in_max, 
                        uint32_t out_min, uint32_t out_max) 
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Use a neopixel to indicate controller status by modifying RGB colors 
 *        and level.
 * @param speed  Ceiling speed (0 to 5).
 * @param light  Light on or off.
 */
static void show_status(uint8_t speed, bool light)
{
#ifdef CONFIG_IDF_TARGET_ESP32C3
    uint32_t level = num_map(speed, 0, MAX_CELING_SPEED, 0, 100);
    // If the fan and the light are activated, it indicates the speed level in 
    // green, and when the light is off in red; If the fan is off and the light 
    // is on, the blue led turns on 100%.
    if ((speed > 0) && (g_power)) {
        if (light) { 
            ws2812_led_set_rgb(0, level, 0);
        } else {
            ws2812_led_set_rgb(level, 0, 0);  
        }
    } else if (light) {
        ws2812_led_set_rgb(0, 0, 100); 
    } else {
        ws2812_led_clear();
    }
#endif
}

/**
 * @brief Control the relays to set the fan speed.
 * @param val Ceiling speed. 0 = turn off, 4 = max.
 */
static void set_speed(uint8_t val)
{
    // First turn off the relay.
    if (val == 0) {
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
    } else if (val == 1) {
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, false);
    } else if (val == 2) {
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, false);
    } else if (val == 3) {
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, false);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, false);
    } else if ((val == 4) || (val == 5)) {
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, false);
    }

    show_status(val, g_light);
}

/**
 * @brief Function called by the encoder component when the user moves the shaft.
 * @param event Contains the position and direction of the encoder.
 */
static void encoder_update(rotenc_event_t event)
{
    uint8_t old_speed = g_speed;

    if (abs(last_encoder_position - event.position) >= 3) {
        if ((event.direction == ROTENC_CW) && (g_speed < MAX_CELING_SPEED)) {
            g_speed++;
        } else if ((event.direction == ROTENC_CCW) && (g_speed > 0)) {
            g_speed--;
        } 

        if (old_speed != g_speed) {
            esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_SPEED),
                    esp_rmaker_int(g_speed));

            if ((old_speed == 0) || ((g_power == false) && (g_speed > 0))) {
                g_power = true;
                esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                        esp_rmaker_bool(g_power));
            } else if (g_speed == 0) {
                g_power = false;
                esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                        esp_rmaker_bool(g_power));
                
            }

            set_speed(g_speed);
        }
     
        last_encoder_position = event.position;
    }
}

/**
 * @brief Initialize the rotary encoder to control speed and light.
 * @param void
 *  
 * @return ESP_OK if successful
 */
static esp_err_t encoder_init(void)
{
    // Initialize the handle instance of the rotary device, 
    // by default it uses 1 mS for the debounce time.
    esp_err_t err = rotenc_init(&h_encoder, 
                                CONFIG_ROT_ENC_CLK_GPIO, 
                                CONFIG_ROT_ENC_DTA_GPIO, 
                                CONFIG_ROT_ENC_DEBOUNCE);

    if (err == ESP_OK) {
        err = rotenc_set_event_callback(&h_encoder, encoder_update);
    }

    return err;
}

/**
 * @brief Function invoked when user presses encoder button to turn light on/off.
 * @param arg
 */
static void push_btn_cb(void *arg)
{
    app_fan_set_ligth(!g_light);
    
    esp_rmaker_param_update_and_report(light_param, esp_rmaker_bool(g_light));
}

/**
 * @brief Function invoked when the timer expires to read the temperature.
 * @param priv
 */
static void app_temperatura_update(void *priv)
{
    app_get_current_temperature();

    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(thermostat_device, ESP_RMAKER_PARAM_TEMPERATURE),
            esp_rmaker_float(g_temperature));

    if (g_temp_enable) {
        if (!g_power) {
            if (g_temperature > g_temp_level) {
                g_power = true;
                esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                    esp_rmaker_bool(g_power));
                set_speed(g_speed);
            }        
        } else {
            if (g_temperature < (g_temp_level + 1)) {
                g_power = false;
                esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                    esp_rmaker_bool(g_power));
                set_speed(0);
            }  
        }
    }
}

/**
 * @brief Initializes the thermostat controller. Initialize the 
 *        thermistor component and install the status update timer.
 * @param void
 * @return ESP_OK if successful
 */
static esp_err_t app_temperature_init(void)
{
    g_temperature = DEFAULT_TEMPERATURE;
    esp_timer_create_args_t temperature_timer_conf = {
        .callback = app_temperatura_update,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "app_temperatura_update"
    };

    esp_err_t err = thermistor_init(&th, THERMISTOR_ADC_CHANNEL, 
                                    CONFIG_THERMISTOR_SERIE_RESISTANCE, 
                                    CONFIG_THERMISTOR_NOMINAL_RESISTANCE, 
                                    CONFIG_THERMISTOR_NOMINAL_TEMPERATURE,
                                    CONFIG_THERMISTOR_BETA_VALUE, 
                                    CONFIG_THERMISTOR_VOLTAGE_SOURCE);

    if (err == ESP_OK) {
        err = esp_timer_create(&temperature_timer_conf, &temperature_timer);
        if (err == ESP_OK) {
            esp_timer_start_periodic(temperature_timer, TEMPERATURE_REPORTING_PERIOD * 1000000U);
        }
    }

    return err;
}

esp_err_t app_fan_set_power(bool power)
{
    g_power = power;
    if (power) {
        set_speed(g_speed);
    } else {
        set_speed(0);
    }
    return ESP_OK;
}

esp_err_t app_fan_set_speed(uint8_t speed)
{
    g_speed = speed;

    if ((g_speed > 0) && !g_power) {
        g_power = true;
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_power));
    } else if ((g_speed == 0) && g_power) {
        g_power = false;
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_power));
        
    }

    set_speed(speed);

    return ESP_OK; 
}

esp_err_t app_fan_set_ligth(bool state)
{
    g_light = state;

    gpio_set_level(CONFIG_RELE_LIGHT_GPIO, !state);

    show_status(g_speed, g_light);
    return ESP_OK;  
}

esp_err_t app_fan_init(void)
{
    app_fan_set_power(g_power);
    return ESP_OK;
}

void app_driver_init()
{
    app_fan_init();
    
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    /* Configure power */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    uint64_t pin_mask = (((uint64_t)1 << CONFIG_RELE_SPEED_CAP_LOW_GPIO) | 
                         ((uint64_t)1 << CONFIG_RELE_SPEED_CAP_HIGH_GPIO) | 
                         ((uint64_t)1 << CONFIG_RELE_SPEED_DIRECT_GPIO) | 
                         ((uint64_t)1 << CONFIG_RELE_LIGHT_GPIO) );
    io_conf.pin_bit_mask = pin_mask;
    /* Configure the GPIO */
    gpio_config(&io_conf);

    app_fan_set_ligth(false);
    set_speed(0); 

    encoder_init(); 
    app_temperature_init(); 
    init_led();
}

float app_get_current_temperature(void)
{
    g_temperature = thermistor_get_celsius(&th);

    return g_temperature;
}

void app_temp_set_enable(bool enable)
{
    g_temp_enable = enable;
}

void app_temp_set_level(int level)
{
    g_temp_level = level;
}