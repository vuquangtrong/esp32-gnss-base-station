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

#include <string.h>
#include <nvs_flash.h>

#include "util.h"
#include "config.h"

#define NVS_NAMESPACE "config"

static const char *TAG = "CONFIG";
static nvs_handle_t nvs = 0;

// ordered status list
static char config[CONFIG_MAX][CONFIG_LEN_MAX];
static char config_name[CONFIG_MAX][CONFIG_LEN_MAX / 2] = {
    "wifi_ssid",
    "wifi_pwd",
};

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

    // erase allocated memory
    memset(config, 0, CONFIG_MAX * CONFIG_LEN_MAX);

    // load from NVS
    size_t len;
    for (size_t type = CONFIG_START; type < CONFIG_MAX; type++)
    {
        nvs_get_str(nvs, config_name[type], NULL, &len);
        if (len > 0)
        {
            nvs_get_str(nvs, config_name[type], config[type], &len);
        }

        ESP_LOGI(TAG, "config_init:\r\nkey=%s\r\nval=%s", config_name[type], config[type]);
    }
    return ESP_OK;
}

void config_set(config_t type, const char *value)
{
    memset(config[type], 0, CONFIG_LEN_MAX);
    strncpy(config[type], value, CONFIG_LEN_MAX);

    // save to NVS
    ESP_LOGI(TAG, "config_set:\r\nkey=%s\r\nval=%s", config_name[type], config[type]);
    nvs_set_str(nvs, config_name[type], value);
    nvs_commit(nvs);
}

char *config_get(config_t type)
{
    return config[type];
}