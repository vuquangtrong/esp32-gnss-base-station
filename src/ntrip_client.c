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
#include "uart.h"
#include "ping.h"
#include "ntrip_client.h"

#define BUFFER_SIZE 2048

static const char *TAG = "NTRIP_CLIENT";
static char *source_table;
static esp_http_client_handle_t ntrip_client = NULL;
static bool isRequestedDisconnect = false;

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

    ESP_LOGI(TAG, "Finish ntrip_get_mnts!");
    vTaskDelete(NULL);
}

void ntrip_client_get_mnts()
{
    xTaskCreate(ntrip_client_get_mnts_task, "ntrip_get_mnts", 8192, NULL, 10, NULL);
}

static void uart_status_read_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (ntrip_client != NULL)
    {
        int sent = esp_http_client_write(ntrip_client, (char *)event_data, event_id);
        ERROR_IF(sent < 0,
                 return,
                 "Cannot write to ntrip caster");
    }
}

static void ntrip_client_stream_task(void *args)
{
    status_set(STATUS_NTRIP_CLI_STATUS, "Connecting");

    char *host = config_get(CONFIG_NTRIP_IP);
    int port = atoi(config_get(CONFIG_NTRIP_PORT));
    if (!port)
        port = 2101;
    char *user = config_get(CONFIG_NTRIP_USER);
    char *pwd = config_get(CONFIG_NTRIP_PWD);
    char *mnt = config_get(CONFIG_NTRIP_MNT);

    char path[128] = "/";
    strcpy(path + 1, mnt);

    // ping_test(host);

    esp_http_client_config_t config = {
        .host = host,
        .port = port,
        .path = path,
        .username = user,
        .password = pwd,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        // .disable_auto_redirect = true,
        // .timeout_ms = 30000,
    };

    ntrip_client = esp_http_client_init(&config);
    // esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36 Edg/114.0.1823.51");
    esp_http_client_set_header(ntrip_client, "User-Agent", "NTRIP GNSS/1.0");
    esp_http_client_set_header(ntrip_client, "Ntrip-Version", "Ntrip/2.0");
    esp_http_client_set_header(ntrip_client, "Connection", "keep-alive");

    esp_err_t err = esp_http_client_open(ntrip_client, 0);
    ERROR_IF(err != ESP_OK,
             goto ntrip_client_stream_task_end,
             "Cannot open %s:%d", host, port);

    uart_register_handler(UART_STATUS_EVENT_READ, uart_status_read_event_handler);

    int32_t content_length = esp_http_client_fetch_headers(ntrip_client);
    ERROR_IF(content_length < 0,
             goto ntrip_client_stream_task_end,
             "Cannot read from %s:%d", host, port);

    int status_code = esp_http_client_get_status_code(ntrip_client);
    ERROR_IF(status_code != 200,
             goto ntrip_client_stream_task_end,
             "Cannot get OK status from %s:%d", host, port);

    ERROR_IF(!esp_http_client_is_chunked_response(ntrip_client),
             goto ntrip_client_stream_task_end,
             "Cannot open stream to %s:%d", host, port);

    status_set(STATUS_NTRIP_CLI_STATUS, "Connected");

    char *buffer = malloc(BUFFER_SIZE);
    int len;
    while ((len = esp_http_client_read(ntrip_client, buffer, BUFFER_SIZE)) >= 0)
    {
        ubx_write_rtcm3(buffer, len);
        if (isRequestedDisconnect)
            break;
    }

    free(buffer);
    uart_unregister_handler(UART_STATUS_EVENT_READ, uart_status_read_event_handler);

ntrip_client_stream_task_end:
    esp_http_client_close(ntrip_client);
    esp_http_client_cleanup(ntrip_client);
    ntrip_client = NULL;

    ESP_LOGI(TAG, "Finish ntrip_stream_task!");
    status_set(STATUS_NTRIP_CLI_STATUS, "Disconnected");

    vTaskDelete(NULL);
}

void ntrip_client_connect()
{
    isRequestedDisconnect = false;
    xTaskCreate(ntrip_client_stream_task, "ntrip_stream_task", 8192, NULL, 10, NULL);
}

void ntrip_client_disconnect()
{
    isRequestedDisconnect = true;
}