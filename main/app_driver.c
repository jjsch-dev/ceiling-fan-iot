/* Celling Fan implementation using reles and thermistor.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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
    #define DEFAULT_HUE         180
    #define DEFAULT_SATURATION  100
    #define DEFAULT_BRIGHTNESS  ( 20 * DEFAULT_SPEED)
#endif

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_ROT_ENC_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static uint8_t g_speed = DEFAULT_SPEED;
static uint16_t g_value = DEFAULT_BRIGHTNESS;
static bool g_power = DEFAULT_POWER;
static bool g_light = false;
static float g_temperature = 0;
static bool g_temp_enable = false;
static int g_temp_level = 0;

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
    if (err == ESP_OK){
        err = ws2812_led_clear();
    }
    ESP_LOGI(TAG, "ws2812_led_init: %d", err);
#endif
    return err;
}

/**
 * @brief With Neopixel, the saturation of color changes with the value of the 
 *        temperature, and with the standard LED toggle each time.
 * @param celius  Temperature in degrees Celsius.
 */
static void speed_to_light(uint speed)
{
#ifdef CONFIG_IDF_TARGET_ESP32C3
    if (speed > 0){
        uint16_t g_hue = (speed * 50);
        ws2812_led_set_hsv(g_hue, DEFAULT_SATURATION, DEFAULT_BRIGHTNESS);
    } else {
        ws2812_led_clear();
    }
#endif
}

static void set_speed(int val)
{
    // First turn off the relay.
    if (val == 0) {
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
    } else if (val == 1){
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, false);
    } else if (val == 2){
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, false);
    } else if (val == 3){
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, false);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, false);
    } else if ((val == 4) || (val == 5)){
        gpio_set_level(CONFIG_RELE_SPEED_CAP_LOW_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_CAP_HIGH_GPIO, true);
        gpio_set_level(CONFIG_RELE_SPEED_DIRECT_GPIO, false);
    }

    speed_to_light(val);
}

static void encoder_update(rotenc_event_t event)
{
    uint8_t old_speed = g_speed;

    if (abs(last_encoder_position - event.position) >= 3){
        if ((event.direction == ROTENC_CW) && (g_speed < 5)) {
            set_speed(++g_speed);
        } else if ((event.direction == ROTENC_CCW) && (g_speed > 0)) {
            set_speed(--g_speed);
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
        }
     
        last_encoder_position = event.position;
    }
}

static int encoder_init(void)
{
    // Initialize the handle instance of the rotary device, 
    // by default it uses 1 mS for the debounce time.
    int err = rotenc_init(&h_encoder, 
                          CONFIG_ROT_ENC_CLK_GPIO, 
                          CONFIG_ROT_ENC_DTA_GPIO, 
                          CONFIG_ROT_ENC_DEBOUNCE);

    if (err == ESP_OK) {
        err = rotenc_set_event_callback(&h_encoder, encoder_update);
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
    g_value = 20 * g_speed;

    set_speed(speed);

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

    return ESP_OK; 
}

esp_err_t app_fan_set_ligth(bool state)
{
    g_light = state;

    gpio_set_level(CONFIG_RELE_LIGHT_GPIO, !state);
    return ESP_OK;  
}

esp_err_t app_fan_init(void)
{
    app_fan_set_power(g_power);
    return ESP_OK;
}

static void push_btn_cb(void *arg)
{
    app_fan_set_ligth(!g_light);
    
    esp_rmaker_param_update_and_report(light_param, esp_rmaker_bool(g_light));
}

static void app_temperatura_update(void *priv)
{
    app_get_current_temperature();

    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(thermostat_device, ESP_RMAKER_PARAM_TEMPERATURE),
            esp_rmaker_float(g_temperature));

    if (g_temp_enable) {
        if (!g_power){
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