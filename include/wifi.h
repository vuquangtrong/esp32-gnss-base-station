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

#ifndef ESP32_GNSS_WIFI_H
#define ESP32_GNSS_WIFI_H

#include <esp_err.h>

#define WIFI_TRIAL_RESET 1
#define WIFI_TRIAL_MAX 5

esp_err_t wifi_init();
esp_err_t wifi_connect(bool reset_trial);
esp_err_t wifi_disconnect();
void wait_for_ip();

#endif // ESP32_GNSS_WIFI_H