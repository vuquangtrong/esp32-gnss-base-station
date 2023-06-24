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

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_server.h>

#include "util.h"
#include "status.h"
#include "uart.h"
#include "ntrip_caster.h"

static const char *TAG = "NTRIP_CASTER";

typedef struct ntrip_caster_client_t
{
    httpd_handle_t hd;
    int socket;
    SLIST_ENTRY(ntrip_caster_client_t)
    next;
} ntrip_caster_client_t;

static SLIST_HEAD(caster_clients_list_t, ntrip_caster_client_t) caster_clients_list;

static char TABLE_RESPONSE[] =
    "SOURCETABLE 200 OK" CARRET NEWLINE
    "Content-Type: text/plain" CARRET NEWLINE
    "Content-Length: 115" CARRET NEWLINE
        CARRET NEWLINE
    "STR;BASE;BASE;RTCM 3;;2;GPS+GLO+GAL+BDS+QZSS;GNSS;VN;21.028511;105.804817;0;0;GNSS;none;N;N;9600;" CARRET NEWLINE
    "ENDSOURCETABLE" CARRET NEWLINE
        CARRET NEWLINE;

static char STREAM_RESPONSE[] =
    "ICY 200 OK" CARRET NEWLINE;

static char client_count = 0;

static esp_err_t mount_table_handler(httpd_req_t *req)
{
    httpd_handle_t hd = req->handle;
    int sockfd = httpd_req_to_sockfd(req);

    httpd_socket_send(hd, sockfd, TABLE_RESPONSE, strlen(TABLE_RESPONSE), MSG_MORE);

    return ESP_OK;
}

static void destroy_socket(int socket)
{
    if (socket < 0)
        return;
    shutdown(socket, SHUT_RDWR);
    close(socket);
}

static void ntrip_caster_client_remove(ntrip_caster_client_t *caster_client)
{
    destroy_socket(caster_client->socket);
    SLIST_REMOVE(&caster_clients_list, caster_client, ntrip_caster_client_t, next);
    free(caster_client);
    client_count--;
    sprintf(status_get(STATUS_NTRIP_CAS_STATUS), "%d", client_count);
}

static void uart_rtcm3_read_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // ESP_LOGW(TAG, "Got RTCM3: %s", (char *)event_data);
    // ESP_LOGW(TAG, "x");
    ntrip_caster_client_t *client, *client_tmp;
    SLIST_FOREACH_SAFE(client, &caster_clients_list, next, client_tmp)
    {
        // ESP_LOGW(TAG, "found socket: %d", client->socket);
        int len = httpd_socket_send(client->hd, client->socket, event_data, event_id, MSG_MORE);
        ERROR_IF(len < 0,
                 ntrip_caster_client_remove(client),
                 "delete socket %d", client->socket);
    }
}

static void custom_httpd_close_func(httpd_handle_t hd, int sockfd)
{
    // if socket is in the streaming list
    bool found = false;
    ntrip_caster_client_t *client, *client_tmp;
    SLIST_FOREACH_SAFE(client, &caster_clients_list, next, client_tmp)
    {
        if (client->socket == sockfd)
        {
            found = true;
        }
    }

    // if not, then close it
    if (!found)
    {
        destroy_socket(sockfd);
    }
}

static esp_err_t base_stream_handler(httpd_req_t *req)
{
    ntrip_caster_client_t *client = malloc(sizeof(ntrip_caster_client_t));
    client->hd = req->handle;
    client->socket = httpd_req_to_sockfd(req);
    ESP_LOGI(TAG, "new socket: %d", client->socket);
    SLIST_INSERT_HEAD(&caster_clients_list, client, next);
    client_count++;
    sprintf(status_get(STATUS_NTRIP_CAS_STATUS), "%d", client_count);

    httpd_socket_send(client->hd, client->socket, STREAM_RESPONSE, strlen(STREAM_RESPONSE), MSG_MORE);

    return ESP_OK;
}

httpd_uri_t _mount_table_handler = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = mount_table_handler,
    .user_ctx = NULL,
};

httpd_uri_t _base_stream_handler = {
    .uri = "/BASE",
    .method = HTTP_GET,
    .handler = base_stream_handler,
    .user_ctx = NULL,
};

esp_err_t custom_httpd_err_func(httpd_req_t *req, httpd_err_code_t error)
{
    int sockfd = httpd_req_to_sockfd(req);

    // if socket is in the streaming list
    bool found = false;
    ntrip_caster_client_t *client, *client_tmp;
    SLIST_FOREACH_SAFE(client, &caster_clients_list, next, client_tmp)
    {
        if (client->socket == sockfd)
        {
            found = true;
        }
    }

    // if it is, keep socket open
    if (found)
    {
        return ESP_OK;
    }

    httpd_resp_send_err(req, error, NULL);
    return ESP_FAIL;
}

esp_err_t ntrip_caster_init()
{
    esp_err_t err = ESP_OK;

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = 2101;
    config.ctrl_port = config.ctrl_port - 1;
    config.lru_purge_enable = true;
    config.keep_alive_enable = true;
    config.keep_alive_interval = 5;
    config.keep_alive_idle = 5;
    config.keep_alive_count = 3;
    config.close_fn = custom_httpd_close_func;

    err = httpd_start(&server, &config);
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Failed to start file server!");

    httpd_register_uri_handler(server, &_mount_table_handler);
    httpd_register_uri_handler(server, &_base_stream_handler);
    httpd_register_err_handler(server, HTTPD_400_BAD_REQUEST, custom_httpd_err_func);
    httpd_register_err_handler(server, HTTPD_501_METHOD_NOT_IMPLEMENTED, custom_httpd_err_func);
    httpd_register_err_handler(server, HTTPD_500_INTERNAL_SERVER_ERROR, custom_httpd_err_func);

    ESP_LOGI(TAG, "Starting NTRIP Server on port %d", config.server_port);

    uart_register_handler(UART_RTCM3_EVENT_READ, uart_rtcm3_read_event_handler);

    return err;
}