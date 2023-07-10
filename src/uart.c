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
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_event.h>

#include "util.h"
#include "config.h"
#include "status.h"
#include "ublox.h"
#include "uart.h"

#define UART_STATUS_BUFFER_LEN 256
#define UART_RTCM3_BUFFER_LEN 2048
#define UBX_MSG_LEN 128

static const char *TAG = "UART";

// UART0 is connected to U-blox UART2, for sending or reading RTCM3
// This UART0 port is also connected to USB-VCOM for upload firmware
ESP_EVENT_DEFINE_BASE(UART_RTCM3_EVENT_READ);
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

    // Enable High Precision mode
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-NMEA-HIGHPREC 1", buffer);
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
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2OUTPROT-NMEA 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // UBX input is enabled, UBX output is disabled by default
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2OUTPROT-UBX 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // RTCM3 input and RTCM3 output are enabled by default
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2OUTPROT-RTCM3X 0", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

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

void ubx_set_mode_rover()
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

    status_set(STATUS_GNSS_MODE, "Rover");
}

void ubx_set_mode_survey(const char *dur, const char *acc)
{
    uint8_t *buffer = calloc(32, sizeof(uint8_t));
    uint32_t n;

    // Survey in 5 mins = 300 seconds
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-SVIN_MIN_DUR 300", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // Accuracy in 5000 x 0.1 = 500 mm = 50 cm
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-SVIN_ACC_LIMIT 5000", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // TMODE Enabled in Survey-in mode
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-MODE 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // Enable RTCM3 output on UART2
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2OUTPROT-RTCM3X 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    free(buffer);

    vTaskDelay(pdMS_TO_TICKS(1000));

    status_set(STATUS_GNSS_MODE, "Base-Survey");
}

static void set_msg_with_val_scale(char *buffer, const char *msg, const char *val, int scale)
{
    char *p;
    int s;
    int l = strlen(msg);

    p = strstr(val, ".");
    s = p - val;

    strcpy(buffer, msg);
    strncpy(buffer + l, val, s);
    strncpy(buffer + l + s, val + s + 1, scale);
}

void ubx_set_mode_fixed(const char *lat, const char *lon, const char *alt)
{
    char *msg = calloc(UBX_MSG_LEN, sizeof(char));
    uint8_t *buffer = calloc(UBX_MSG_LEN, sizeof(uint8_t));
    uint32_t n;

    // POS LLH
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-POS_TYPE 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // LAT in scale of 10^-7
    memset(msg, 0, UBX_MSG_LEN);
    set_msg_with_val_scale(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-LAT ", lat, 7);
    n = ubx_gen_cmd(msg, buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    memset(msg, 0, UBX_MSG_LEN);
    sprintf(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-LAT_HP %s", &lat[strlen(lat)-2]);
    n = ubx_gen_cmd(msg, buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // LON in scale of 10^-7
    memset(msg, 0, UBX_MSG_LEN);
    set_msg_with_val_scale(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-LON ", lon, 7);
    n = ubx_gen_cmd(msg, buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    memset(msg, 0, UBX_MSG_LEN);
    sprintf(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-LON_HP %s", &lon[strlen(lon)-2]);
    n = ubx_gen_cmd(msg, buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // HEIGHT in cm
    memset(msg, 0, UBX_MSG_LEN);
    set_msg_with_val_scale(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-HEIGHT ", alt, 2);
    n = ubx_gen_cmd(msg, buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    memset(msg, 0, UBX_MSG_LEN);
    sprintf(msg, "CFG-VALSET 0 1 0 0 CFG-TMODE-HEIGHT_HP %s0", &alt[strlen(alt)-1]);
    n = ubx_gen_cmd(msg, buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // ACC = 500 x 0.1 = 50mm = 5 cm
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-FIXED_POS_ACC 500", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // TMODE Enabled in Fixed mode
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-TMODE-MODE 2", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    // Enable RTCM3 output on UART2
    n = ubx_gen_cmd("CFG-VALSET 0 1 0 0 CFG-UART2OUTPROT-RTCM3X 1", buffer);
    uart_write_bytes(UART_STATUS_PORT, buffer, n);

    free(buffer);

    vTaskDelay(pdMS_TO_TICKS(1000));

    status_set(STATUS_GNSS_MODE, "Base-Fixed");
}

void ubx_write_rtcm3(const char *buffer, size_t len)
{
    uart_write_bytes(UART_RTCM3_PORT, buffer, len);
}

static void uart_status_task(void *ctx)
{
    char *buffer = calloc(UART_STATUS_BUFFER_LEN, sizeof(char));
    int32_t len;
    char *ptr;

    ESP_LOGI(TAG, "Start uart_status_task");
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

static void uart_rtcm3_task(void *ctx)
{
    char *buffer = calloc(UART_STATUS_BUFFER_LEN, sizeof(char));
    int32_t len;

    ESP_LOGI(TAG, "Start uart_rtcm3_task");
    while (true)
    {
        len = uart_read_bytes(UART_RTCM3_PORT, buffer, UART_STATUS_BUFFER_LEN, pdMS_TO_TICKS(500));
        if (len > 0)
        {
            esp_event_post(UART_RTCM3_EVENT_READ, len /* use len as event ID */, buffer, len, portMAX_DELAY);
        }
        else // keep sockets alive
        {
            esp_event_post(UART_RTCM3_EVENT_READ, 4, "GNSS", 4, portMAX_DELAY);
        }
    }
}

esp_err_t uart_init()
{
    esp_err_t err = ESP_OK;

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

    /*
     * start reading tasks
     */
    xTaskCreate(uart_status_task, "uart_status", 2048, NULL, 10, NULL);
    xTaskCreate(uart_rtcm3_task, "uart_rtcm3", 2048, NULL, 10, NULL);
    return err;
}
