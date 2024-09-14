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

#ifndef ESP32_GNSS_UART_H
#define ESP32_GNSS_UART_H

#include <driver/uart.h>
#include <esp_err.h>
#include <esp_event.h>

extern esp_event_base_t const UART_RTCM3_EVENT_READ;
extern esp_event_base_t const UART_RTCM3_EVENT_WRITE;
extern esp_event_base_t const UART_STATUS_EVENT_READ;
extern esp_event_base_t const UART_STATUS_EVENT_WRITE;

esp_err_t uart_init();

void uart_register_handler(esp_event_base_t event_base, esp_event_handler_t event_handler);
void uart_unregister_handler(esp_event_base_t event_base, esp_event_handler_t event_handler);

void ubx_set_default();
void ubx_set_mode_rover();
void ubx_set_mode_survey(const char* dur, const char* acc);
void ubx_set_mode_fixed(const char* lat, const char* lon, const char* alt);
void ubx_write_rtcm3(const char* buffer, size_t len);

#endif // ESP32_GNSS_UART_H