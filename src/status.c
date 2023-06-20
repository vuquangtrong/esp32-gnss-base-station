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

#include "util.h"
#include "config.h"
#include "status.h"

static const char *TAG = "STATUS";

// ordered status list
static char status[STATUS_MAX][STATUS_LEN_MAX];

esp_err_t status_init()
{
    // clear allocated memory
    memset(status, 0, STATUS_MAX * STATUS_LEN_MAX);
    return ESP_OK;
}

void status_set(status_t type, const char *value)
{
    memset(status[type], 0, STATUS_LEN_MAX);
    strncpy(status[type], value, STATUS_LEN_MAX);
}

char *status_get(status_t type)
{
    return status[type];
}