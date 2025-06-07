/*
 * This file is part of the ESP32-GNSS-Base-Station firmware, published
 * at (https://github.com/vuquangtrong/esp32-gnss-base-station).
 * Copyright (c) 2023 Vu Quang Trong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "battery.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_event.h>
#include <max17048.h>

#include "util.h"
#include "config.h"
#include "status.h"

static const char *TAG = "BATT";

static i2c_bus_handle_t i2c_bus = NULL;
static max17048_handle_t max17048 = NULL;
const i2c_config_t I2C_MASTER_CONFIG = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = GPIO_NUM_22,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400 * 1000,
};

static void battery_task(void *arg)
{
    float last_vol = 0.0f, curr_vol = 0.0f;
    float last_soc = 0.0f, curr_soc = 0.0f;
    char buffer[4] = {0};

    ESP_LOGI(TAG, "Start battery_task");
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        max17048_get_cell_voltage(max17048, &curr_vol);
        vTaskDelay(pdMS_TO_TICKS(100));
        max17048_get_cell_percent(max17048, &curr_soc);
        if (curr_soc != last_soc)
        {
            last_soc = curr_soc;
            ESP_LOGI(TAG, "%.2f%%", curr_soc);
            snprintf(buffer, 4, "%3d", (int)curr_soc);
            status_set(STATUS_BATTERY, buffer);
        }
    }
}

esp_err_t battery_init()
{

    i2c_bus = i2c_bus_create(I2C_NUM_0, &I2C_MASTER_CONFIG);
    max17048 = max17048_create(i2c_bus, MAX17048_I2C_ADDR_DEFAULT);

    if (max17048 == NULL)
    {
        ESP_LOGE(TAG, "Failed to create MAX17048 handle");
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskCreate(battery_task, "battery_task", 2048, NULL, 10, NULL);
    return ESP_OK;
}
