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

#ifndef ESP32_GNSS_STATUS_H
#define ESP32_GNSS_STATUS_H

#include <esp_err.h>

#define STATUS_LEN_MAX 128

typedef enum
{
    STATUS_START = 0,
    STATUS_GNSS_STATUS = STATUS_START,
    STATUS_GNSS_MODE,
    STATUS_NTRIP_CLI_STATUS,
    STATUS_NTRIP_CAS_STATUS,
    STATUS_WIFI_STATUS,
    STATUS_MAX
} status_t;

esp_err_t status_init();
void status_set(status_t type, const char *value);
char *status_get(status_t type);

#endif // ESP32_GNSS_STATUS_H