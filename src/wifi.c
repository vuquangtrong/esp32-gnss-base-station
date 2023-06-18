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
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_mac.h>
#include <esp_wifi.h>

#include "util.h"
#include "config.h"
#include "wifi.h"

static const char *TAG = "WIFI";

static EventGroupHandle_t wifi_event_group;
static const int WIFI_STA_GOT_IP_BIT = BIT0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_err_t err = ESP_OK;

    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_AP_STACONNECTED)
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Device " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        }
        else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
        {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Device " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        }
        else if (event_id == WIFI_EVENT_STA_START)
        {
            err = esp_wifi_connect();
            ERROR_IF(err != ESP_OK,
                     return,
                     "Cannot connect WiFi");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            xEventGroupClearBits(wifi_event_group, WIFI_STA_GOT_IP_BIT);
            err = esp_wifi_connect();
            ERROR_IF(err != ESP_OK,
                     return,
                     "Cannot connect WiFi");
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_STA_GOT_IP_BIT);
            char buffer[16];
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            snprintf(buffer, 16, IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Connected to Wifi. IP=%s", buffer);
        }
        else if (event_id == IP_EVENT_STA_LOST_IP)
        {
            xEventGroupClearBits(wifi_event_group, WIFI_STA_GOT_IP_BIT);
        }
    }
}

esp_err_t wifi_init()
{
    esp_err_t err = ESP_OK;

    wifi_event_group = xEventGroupCreate();

    // start network interface
    err = esp_netif_init();
    ERROR_IF(err != ESP_OK,
             return err,
             "Cannot start NetIF");

    // start a default AP interface
    esp_netif_t *netif_wifi_ap = esp_netif_create_default_wifi_ap();
    ERROR_IF(netif_wifi_ap == NULL,
             return ESP_FAIL,
             "Cannot start Wifi AP NetIF");

    // start a default STA interface
    esp_netif_t *netif_wifi_sta = esp_netif_create_default_wifi_sta();
    ERROR_IF(netif_wifi_sta == NULL,
             return ESP_FAIL,
             "Cannot start WiFi STA NetIF");

    // load default wifi configs
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_config);
    ERROR_IF(err != ESP_OK,
             return err,
             "Cannot start WiFi");

    // register events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // set WiFi mode
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Cannot set Wifi mode");
    err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    err = esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    err = esp_wifi_set_ps(WIFI_PS_NONE);

    // get WiFi MAC
    uint8_t mac[6];
    err = esp_wifi_get_mac(WIFI_IF_AP, mac);
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Cannot get Wifi MAC");

    // config WiFi
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    // WiFi AP configs
    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "GNSS_Base_%02X%02X%02X", mac[3], mac[4], mac[5]);
    snprintf((char *)wifi_config.ap.password, sizeof(wifi_config.ap.password), "12345678");
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.max_connection = 5;
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Cannot set Wifi AP config");

    // WiFi STA configs
    // TODO: load saved wifi config from NVS
    char *username = NULL;
    char *password = NULL;

    if (username && password)
    {
        snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), username);
        snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), password);
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        ERROR_IF(err != ESP_OK,
                 return ESP_FAIL,
                 "Cannot set Wifi STA config");
    }

    err = esp_wifi_start();
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Cannot start Wifi");

    return ESP_OK;
}

void wait_for_ip()
{
    xEventGroupWaitBits(wifi_event_group, WIFI_STA_GOT_IP_BIT, false, false, portMAX_DELAY);
}
