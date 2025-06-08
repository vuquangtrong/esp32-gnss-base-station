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

#ifndef ESP32_GNSS_SDCARD_H
#define ESP32_GNSS_SDCARD_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdio.h>

esp_err_t sdcard_init(void);
esp_err_t sdcard_list_files(const char *path);
esp_err_t sdcard_get_space(uint64_t *total_bytes, uint64_t *free_bytes);

FILE *sdcard_open_file(const char *filepath, bool overwrite);
esp_err_t sdcard_close_file(FILE *file);
esp_err_t sdcard_file_write(FILE *file, const void *data, size_t size);

#endif // ESP32_GNSS_SDCARD_H
