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

#ifndef ESP32_GNSS_UTIL_H
#define ESP32_GNSS_UTIL_H

#include <esp_log.h>

/*
 * remove code of unused log macros
 */

#ifdef ESP_LOGD
#undef ESP_LOGD
#define ESP_LOGD(tag, format, ...) \
    {                              \
    }
#endif

#ifdef ESP_LOGV
#undef ESP_LOGV
#define ESP_LOGV(tag, format, ...) \
    {                              \
    }
#endif

// log ERROR with more info
#define ERROR(format, ...) \
    ESP_LOGE(TAG, "%s:%d (%s): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

// log ERROR on Condition
#define ERROR_IF(condition, action, format, ...)                                               \
    if ((condition))                                                                           \
    {                                                                                          \
        ESP_LOGE(TAG, "%s:%d (%s): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        action;                                                                                \
    }

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define NEWLINE "\n"
#define CARRET "\r"

#endif // ESP32_GNSS_UTIL_H