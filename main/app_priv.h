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
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_POWER                       false
#define DEFAULT_SPEED                       3
#define DEFAULT_LIGHT                       false
#define DEFAULT_TEMPERATURE                 25.0
#define DEFAULT_THERMOSTAT_TEMPERATURE      30
#define DEFAULT_THERMOSTAT_ENABLE           false
#define TEMPERATURE_REPORTING_PERIOD        60 /* Seconds */
#define MAX_CELING_SPEED                    5

#define LIGHT_SWITCH_NAME                   "Ligth"
#define THERMOSTAT_DEVICE_NAME              "Thermostat"
#define THERMOSTAT_SWITCH_NAME              "Enable"
#define THERMOSTAT_SLIDER_NAME              "Temp"

extern esp_rmaker_device_t *fan_device;
extern esp_rmaker_param_t *light_param;

extern esp_rmaker_device_t *thermostat_device;
extern esp_rmaker_param_t *thermostat_enable_param;
extern esp_rmaker_param_t *thermostat_slider_param;

/**
 * @brief Initializes the encoder, the thermistor, the relays and the led.
 * @param void.
 */
void app_driver_init(void);

/**
 * @brief Turn the ceiling fan on and off.
 * @param power True = ON.
 *  
 * @return ESP_OK if successful
 */
esp_err_t app_fan_set_power(bool power);


/**
 * @brief TControl the fan speed.
 * @param Ceiling speed. 0 = turn off, 4 = max.
 *  
 * @return ESP_OK if successful
 */
esp_err_t app_fan_set_speed(uint8_t speed);

/**
 * @brief Turn the fan light on and off.
 * @param state True = ON
 *  
 * @return ESP_OK if successful
 */
esp_err_t app_fan_set_ligth(bool state);

/**
 * @brief Get the current temperature..
 * @param void
 *  
 * @return Celsius degrees.
 */
float app_get_current_temperature(void);

/**
 * @brief Enable temperature fan control.
 * @param enable True = Enabled
 */
void app_temp_set_enable(bool enable);

/**
 * @brief Set the thermostat temperature.
 * @param level = 20 to 50 Celsius degree.
 */
void app_temp_set_level(int level);
