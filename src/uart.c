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

#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_event.h>

#include "util.h"
#include "config.h"
#include "status.h"
#include "ublox.h"
#include "uart.h"

#define UART_STATUS_BUFFER_LEN 256
#define UART_RTCM3_BUFFER_LEN 2048

static const char *TAG = "UART";

static EventGroupHandle_t uart_event_group;
static const int UBLOX_UART_STATUS_READY_BIT = BIT0;
static const int UBLOX_UART_RTCM3_READY_BIT = BIT1;

// UART0 is connected to U-blox UART2, for sending or reading RTCM3
// This UART0 port is also connected to USB-VCOM for upload firmware
ESP_EVENT_DEFINE_BASE(UART_RTCM3_EVENT_READ);
ESP_EVENT_DEFINE_BASE(UART_RTCM3_EVENT_WRITE);
const uart_port_t UART_RTCM3_PORT = UART_NUM_0;
const uint8_t UART_RTCM3_PIN_TX = GPIO_NUM_1;
const uint8_t UART_RTCM3_PIN_RX = GPIO_NUM_3;
const uart_config_t UART_RTCM3_CONFIG = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

// UART1 is connected to U-blox UART1, for sending CFG, and reading GGA
ESP_EVENT_DEFINE_BASE(UART_STATUS_EVENT_READ);
ESP_EVENT_DEFINE_BASE(UART_STATUS_EVENT_WRITE);
const uart_port_t UART_STATUS_PORT = UART_NUM_1;
const uint8_t UART_STATUS_PIN_TX = GPIO_NUM_13;
const uint8_t UART_STATUS_PIN_RX = GPIO_NUM_19;
const uart_config_t UART_STATUS_CONFIG = {
    .baud_rate = 38400,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

void uart_register_handler(esp_event_base_t event_base, esp_event_handler_t event_handler)
{
    esp_event_handler_register(event_base, ESP_EVENT_ANY_ID, event_handler, NULL);
}

void uart_unregister_handler(esp_event_base_t event_base, esp_event_handler_t event_handler)
{
    esp_event_handler_unregister(event_base, ESP_EVENT_ANY_ID, event_handler);
}

void ubx_set_default()
{
    uint8_t *buffer = calloc(32, sizeof(uint8_t));
    uint32_t n;

    /*
     * UART 1
     */
    // NMEA ouput is enabled by default; only keep GGA; disable GLL, GSA, GSV, RMC, VTG, TXT
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-NMEA_ID_GLL_UART1 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-NMEA_ID_GSA_UART1 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-NMEA_ID_GSV_UART1 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-NMEA_ID_RMC_UART1 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-NMEA_ID_VTG_UART1 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-NMEA_ID_TXT_UART1 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // RTCM3 input/output should be disabled
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART1INPROT-RTCM3X 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART1OUTPROT-RTCM3X 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    /*
     * UART 2
     */
    // Set Baudraet
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2-BAUDRATE 115200", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // NMEA input and NMEA output are disabled by default

    // UBX input is enabled, UBX output is disabled by default

    // RTCM3 input and RTCM3 output are enabled by default
    // default measurement rate is 1 Hz
    // n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-RATE-MEAS 1", buffer);
    // uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // set output rate of recommended RTCM3 messages
    //// RTCM 1005 Stationary RTK reference station ARP
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-RTCM_3X_TYPE1005_UART2 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    //// RTCM 1074 GPS MSM4
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-RTCM_3X_TYPE1074_UART2 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    //// RTCM 1084 GLONASS MSM4
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-RTCM_3X_TYPE1084_UART2 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    //// RTCM 1094 Galileo MSM4
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-RTCM_3X_TYPE1094_UART2 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    //// RTCM 1124 BeiDou MSM4
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-RTCM_3X_TYPE1124_UART2 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);
    //// RTCM 1230 GLONASS code-phase biases
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-MSGOUT-RTCM_3X_TYPE1230_UART2 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    /*
     * MODE
     */
    // default in rover mode
    ubx_set_mode_rover();

    free(buffer);
}

void ubx_set_mode_rover(gnss_mode_t mode)
{
    uint8_t *buffer = calloc(32, sizeof(uint8_t));
    uint32_t n;

    // TMODE Disabled
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-MODE 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // Disable RTCM3 output on UART2
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2OUTPROT-RTCM3X 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    free(buffer);

    vTaskDelay(pdMS_TO_TICKS(1000));
}

void ubx_set_mode_survry(uint8_t min_dur, uint8_t acc_limit)
{
}

void ubx_set_mode_fixed(int32_t lat, int32_t lon, int32_t alt, int32_t lat_hp, int32_t lon_hp, int32_t alt_hp, int32_t pos_acc)
{
}

static void uart_status_task(void *ctx)
{
    char *buffer = calloc(UART_STATUS_BUFFER_LEN, sizeof(char));
    int32_t len;
    char *ptr;

    while (true)
    {
        // read a line
        ptr = buffer;
        while (1)
        {
            if (uart_read_bytes(UART_STATUS_PORT, ptr, 1, portMAX_DELAY) == 1)
            {
                if (*ptr == '\n')
                {
                    *ptr = 0;
                    break;
                }
                ptr++;
            }
        }

        // replace '\r' by '\0'
        len = ptr - buffer - 1;
        buffer[len] = '\0';

        //  if a GGA message
        if (len > 5 && buffer[0] == '$' && buffer[3] == 'G' && buffer[4] == 'G' && buffer[5] == 'A')
        {
            status_set(STATUS_GNSS_STATUS, buffer);
            esp_event_post(UART_STATUS_EVENT_READ, len /* use len as event ID */, buffer, len, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t uart_init()
{
    esp_err_t err = ESP_OK;

    uart_event_group = xEventGroupCreate();

    /*
     * start UART_STATUS port
     */

    // apply config
    err = uart_param_config(UART_STATUS_PORT, &UART_STATUS_CONFIG);
    // assign pins for TX, RX; do not use RTS, CTS
    err = uart_set_pin(UART_STATUS_PORT,
                       UART_STATUS_PIN_TX, UART_STATUS_PIN_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // start driver, RX buffer = 2048, no TX  buffer, no UART queue, no UART event
    err = uart_driver_install(UART_STATUS_PORT, 2048, 0, 0, NULL, 0);
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Cannot start UART_STATUS");

    vTaskDelay(pdMS_TO_TICKS(1000));
    xEventGroupSetBits(uart_event_group, UBLOX_UART_STATUS_READY_BIT);

    // initialize Ublox
    ubx_set_default();

    /*
     * start UART_RTCM3 port
     */

    // apply config
    err = uart_param_config(UART_RTCM3_PORT, &UART_RTCM3_CONFIG);
    // assign pins for TX, RX; do not use RTS, CTS
    err = uart_set_pin(UART_RTCM3_PORT,
                       UART_RTCM3_PIN_TX, UART_RTCM3_PIN_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // start driver, RX buffer = 2048, no TX  buffer, no UART queue, no UART event
    err = uart_driver_install(UART_RTCM3_PORT, 2048, 0, 0, NULL, 0);
    ERROR_IF(err != ESP_OK,
             return ESP_FAIL,
             "Cannot start UART_RTCM3");

    vTaskDelay(pdMS_TO_TICKS(1000));
    xEventGroupSetBits(uart_event_group, UBLOX_UART_RTCM3_READY_BIT);


    /*
     * start reading tasks 
     */
    xTaskCreate(uart_status_task, "uart_status", 2048, NULL, 10, NULL);
    return err;
}
