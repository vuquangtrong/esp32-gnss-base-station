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

#include <esp_spiffs.h>
#include <esp_vfs.h>
#include <esp_http_server.h>

#include "util.h"
#include "config.h"
#include "web_app.h"

#define WWW_PATH_BASE "/www"
#define WWW_PARTITION "www"
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define FILE_HASH_SUFFIX ".crc"
#define BUFFER_SIZE 2048
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

static const char *TAG = "WEB_APP";

static esp_err_t spiffs_init()
{
    esp_err_t err = ESP_OK;

    esp_vfs_spiffs_conf_t conf = {
        .base_path = WWW_PATH_BASE,
        .partition_label = WWW_PARTITION,
        .max_files = 5,
        .format_if_mount_failed = false};

    err = esp_vfs_spiffs_register(&conf);
    ERROR_IF(err != ESP_OK,
             return err,
             "Cannot register SPIFFS");

    vTaskDelay(pdMS_TO_TICKS(1000));
    ERROR_IF(!esp_spiffs_mounted(WWW_PARTITION),
             return ESP_FAIL,
             "SPIFFS partition is not mounted")

    return ESP_OK;
}

static void get_path_from_uri(const char *uri, const char *base_path, char *file_path)
{
    const size_t base_path_len = strlen(base_path);
    size_t path_len = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        path_len = MIN(path_len, quest - uri);
    }

    const char *hash = strchr(uri, '#');
    if (hash)
    {
        path_len = MIN(path_len, hash - uri);
    }

    strcpy(file_path, base_path);
    strncpy(file_path + base_path_len, uri, path_len + 1);
}

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *file_name)
{
    if (IS_FILE_EXT(file_name, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(file_name, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(file_name, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(file_name, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

static esp_err_t check_file_etag(httpd_req_t *req, const char *file_name, char *etag)
{
    esp_err_t err = ESP_OK;

    char *file_hash = calloc(FILE_PATH_MAX, sizeof(char));
    strcpy(file_hash, file_name);
    strcpy(&file_hash[strlen(file_hash)], FILE_HASH_SUFFIX);
    ESP_LOGD(TAG, "file_hash: %s", file_hash);

    // check if file exists or not
    struct stat file_stat;
    if (stat(file_hash, &file_stat) == -1)
    {
        err = ESP_ERR_NOT_FOUND;
        goto check_file_etag_end;
    }

    // open file
    FILE *fd_hash = fopen(file_hash, "r");
    if (fd_hash == NULL)
    {
        err = ESP_FAIL;
        goto check_file_etag_end;
    }

    // read etag from file
    int n = fread(etag, sizeof(char), 8, fd_hash);
    ESP_LOGD(TAG, "etag: %s", etag);
    fclose(fd_hash);

    if (n != 8)
    {
        err = ESP_FAIL;
        goto check_file_etag_end;
    }

    // Compare to header sent by client
    size_t if_none_match_length = httpd_req_get_hdr_value_len(req, "If-None-Match") + 1;
    if (if_none_match_length > 1)
    {
        char *if_none_match = calloc(if_none_match_length, sizeof(char));
        httpd_req_get_hdr_value_str(req, "If-None-Match", if_none_match, if_none_match_length);
        ESP_LOGD(TAG, "If-None-Match: %s", if_none_match);

        bool etag_matched = (strcmp(etag, if_none_match) == 0);
        free(if_none_match);

        if (etag_matched)
        {
            err = ESP_OK;
            goto check_file_etag_end;
        }
        else
        {
            err = ESP_ERR_INVALID_CRC;
            goto check_file_etag_end;
        }
    }

    err = ESP_FAIL;

check_file_etag_end:
    free(file_hash);
    return err;
}

static esp_err_t file_get_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_OK;

    ESP_LOGD(TAG, "uri: %s", req->uri);

    // extract file path
    char *file_path = calloc(FILE_PATH_MAX, sizeof(char));
    get_path_from_uri(req->uri, WWW_PATH_BASE, file_path);
    ESP_LOGD(TAG, "file_path: %s", file_path);

    // if request a directory, reponse with an index page
    size_t file_path_len = strlen(file_path);
    if (file_path[file_path_len - 1] == '/')
    {
        strcpy(&file_path[file_path_len], "index.html");
    }
    ESP_LOGD(TAG, "file_path: %s", file_path);

    // set file type
    set_content_type_from_file(req, file_path);

    // check if file exists or not
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1)
    {
        err = httpd_resp_send_404(req);
        goto file_get_handler_end;
    }

    // check if etag is matched or not
    char etag[] = "00000000";
    if (check_file_etag(req, file_path, etag) == ESP_OK)
    {
        err = httpd_resp_set_status(req, "304 Not Modified");
        err = httpd_resp_send(req, NULL, 0);
        goto file_get_handler_end;
    }

    // if not matched, send the etag
    httpd_resp_set_hdr(req, "ETag", etag);

    // then send the file
    FILE *fd = fopen(file_path, "r");
    if (fd == NULL)
    {
        err = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not read file");
        goto file_get_handler_end;
    }

    err = httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char *buffer = calloc(BUFFER_SIZE, sizeof(char));
    size_t length;
    do
    {
        length = fread(buffer, 1, BUFFER_SIZE, fd);
        err = httpd_resp_send_chunk(req, buffer, length);
        if (err != ESP_OK)
        {
            err = httpd_resp_send_chunk(req, buffer, 0);
            err = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            goto file_get_handler_close;
        }
    } while (length != 0);

file_get_handler_close:
    fclose(fd);
    free(buffer);

file_get_handler_end:
    free(file_path);
    return err;
}

httpd_uri_t _file_get_handler = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = file_get_handler,
    .user_ctx = NULL,
};

static esp_err_t server_init()
{
    esp_err_t err = ESP_OK;

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = 80;
    config.ctrl_port = 8080;

    err = httpd_start(&server, &config);
    ERROR_IF(err != ESP_OK,
             return err,
             "Cannot start HTTP Server at %d for Web App", config.server_port);

    httpd_register_uri_handler(server, &_file_get_handler);

    ESP_LOGI(TAG, "HTTP Web App server is running at port %d", config.server_port);
    return ESP_OK;
}

esp_err_t web_app_init()
{
    esp_err_t err = ESP_OK;
    err = spiffs_init();
    ERROR_IF(err != ESP_OK,
             return err,
             "Cannot init SPIFFS");

    err = server_init();
    ERROR_IF(err != ESP_OK,
             return err,
             "Cannot init HTTP Server");
    return ESP_OK;
}