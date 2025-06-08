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

#include <esp_event.h>

#include "util.h"
#include "config.h"
#include "status.h"
#include "wifi.h"
#include "web_app.h"
#include "uart.h"
#include "ping.h"
#include "ntrip_caster.h"
#include "ntrip_client.h"
#include "battery.h"
#include "sdcard.h"

static const char *TAG = "MAIN";

void app_main()
{
    // create a default event loop for all tasks
    esp_event_loop_create_default();

    // init NVS and load default settings
    config_init();

    // init status
    status_init();

    // start WiFi AP+STA mode
    wifi_init();

    // start Web App
    web_app_init();

    // start UART ports
    uart_init();

    // start battery monitor
    battery_init();

    // initialize SD card
    // sdcard_init();
    // sdcard_get_space(NULL, NULL);
    // sdcard_list_files("/");

    // start NTRIP Caster
    ntrip_caster_init();

    // wait for internet
    wait_for_ip();
    ping(config_get(CONFIG_NTRIP_IP));

    // init ntrip client
    ntrip_client_init();
}
