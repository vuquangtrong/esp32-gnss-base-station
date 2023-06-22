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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_client.h>

#include "util.h"
#include "config.h"
#include "status.h"
#include "ping.h"
#include "ntrip_client.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "NTRIP_CLIENT";
static char *source_table;

esp_err_t ntrip_client_init()
{
    esp_err_t err = ESP_OK;

    source_table = calloc(1024, sizeof(char));
    source_table[0] = '0';  // indicate that source table is not valid
    source_table[1] = '\r'; // indicate that source table is not valid

    return err;
}

char *ntrip_client_source_table()
{
    return source_table;
}

static void ntrip_client_get_mnts_task(void *args)
{
    char *host = config_get(CONFIG_NTRIP_IP);
    int port = atoi(config_get(CONFIG_NTRIP_PORT));
    if (!port)
        port = 2101;
    char *user = config_get(CONFIG_NTRIP_USER);
    char *pwd = config_get(CONFIG_NTRIP_PWD);

    // ping_test(host);

    esp_http_client_config_t config = {
        .host = host,
        .port = port,
        .path = "/",
        .username = user,
        .password = pwd,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        // .disable_auto_redirect = true,
        // .timeout_ms = 30000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    // esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36 Edg/114.0.1823.51");
    esp_http_client_set_header(client, "User-Agent", "NTRIP GNSS/1.0");
    esp_http_client_set_header(client, "Ntrip-Version", "Ntrip/2.0");
    esp_http_client_set_header(client, "Connection", "close");

    esp_err_t err = esp_http_client_open(client, 0);
    ERROR_IF(err != ESP_OK,
             goto ntrip_client_get_mnts_end,
             "Cannot open %s:%d", host, port);

    int32_t content_length = esp_http_client_fetch_headers(client);
    if (content_length > 0)
    {
        char *buffer = malloc(content_length);
        int32_t len = esp_http_client_read_response(client, buffer, content_length);
        if (len > 0)
        {
            // process source table
            char **table = calloc(100, sizeof(char *));
            char *p = buffer;
            int n = 0;
            while ((p = strstr(p, "STR;")) != NULL)
            {
                p += 4; // skip STR;
                char *s = strstr(p, ";");
                *s = '\0'; // terminate string
                table[n++] = p;
                p = s + 1;
            }

            source_table[0] = '0'; // not valid
            p = source_table + 2;
            for (int i = 0; i < n; i++)
            {
                strcpy(p, table[i]);
                int l = strlen(p);
                *(p + l) = '\r';
                p += (l + 1);
            }

            source_table[0] = '1'; // valid now
            free(table);
        }
        else
        {
            ESP_LOGE(TAG, "Cannot read data from %s:%d", host, port);
        }

        free(buffer);
    }
    else
    {
        ESP_LOGE(TAG, "Cannot fetch data from %s:%d", host, port);
    }

ntrip_client_get_mnts_end:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    vTaskDelete(NULL);
}

void ntrip_client_get_mnts()
{
    xTaskCreate(ntrip_client_get_mnts_task, "ntrip_get_mnts", 8192, NULL, 10, NULL);
}

void ntrip_client_connect()
{
}