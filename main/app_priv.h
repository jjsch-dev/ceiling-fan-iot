/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_POWER                     false
#define DEFAULT_SPEED                     3
#define DEFAULT_TEMPERATURE               25.0
#define TEMPERATURE_REPORTING_PERIOD      10 /* Seconds */

#define LIGHT_SWITCH_NAME                 "Ligth"
#define THERMOSTAT_DEVICE_NAME            "Thermostat"
#define THERMOSTAT_SWITCH_NAME            "Enable"
#define THERMOSTAT_SLIDER_NAME            "Temp"

extern esp_rmaker_device_t *fan_device;
extern esp_rmaker_param_t *light_param;

extern esp_rmaker_device_t *thermostat_device;
extern esp_rmaker_param_t *thermostat_enable_param;
extern esp_rmaker_param_t *thermostat_slider_param;

void app_driver_init(void);
esp_err_t app_fan_set_power(bool power);
esp_err_t app_fan_set_speed(uint8_t speed);
esp_err_t app_fan_set_ligth(bool state);
float app_get_current_temperature(void);
void app_temp_set_enable(bool enable);
void app_temp_set_level(int level);
