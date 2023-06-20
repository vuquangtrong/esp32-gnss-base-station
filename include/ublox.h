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

#ifndef ESP32_GNSS_UBLOX_H
#define ESP32_GNSS_UBLOX_H

#include <stdint.h>

typedef enum
{
    GNSS_MODE_ROVER = 0,
    GNSS_MODE_SURVEY,
    GNSS_MODE_FIXED
} gnss_mode_t;

uint32_t ubx_gen_cmd(const char *msg, uint8_t *buff);

#endif // ESP32_GNSS_UBLOX_H