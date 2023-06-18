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

#include <nvs_flash.h>

#include "util.h"
#include "config.h"

#define NVS_NAMESPACE "config"

static const char *TAG = "CONFIG";
static nvs_handle_t nvs = 0;

esp_err_t config_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {

        err = nvs_flash_erase();
        ERROR_IF(err != ESP_OK,
                 return err,
                 "Can not erase NVS!");

        err = nvs_flash_init();
        ERROR_IF(err != ESP_OK,
                 return err,
                 "Can not init NVS!");
    }

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    ERROR_IF(err != ESP_OK,
             return err,
             "Can not open NVS!");

    return ESP_OK;
}