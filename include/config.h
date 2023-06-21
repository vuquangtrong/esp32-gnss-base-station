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

#ifndef ESP32_GNSS_CONFIG_H
#define ESP32_GNSS_CONFIG_H

#include <esp_err.h>

#define CONFIG_LEN_MAX 128

typedef enum
{
    CONFIG_START = 0,
    CONFIG_WIFI_SSID = CONFIG_START,
    CONFIG_WIFI_PWD,
    CONFIG_NTRIP_IP,
    CONFIG_NTRIP_PORT,
    CONFIG_NTRIP_USER,
    CONFIG_NTRIP_PWD,
    CONFIG_NTRIP_MNT,
    CONFIG_BASE_LAT,
    CONFIG_BASE_LON,
    CONFIG_BASE_ALT,
    CONFIG_MAX
} config_t;

esp_err_t config_init();
void config_set(config_t type, const char *value);
char *config_get(config_t type);

#endif // ESP32_GNSS_CONFIG_H