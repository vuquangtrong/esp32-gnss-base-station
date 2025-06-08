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
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "config.h"
#include "sdcard.h"

static const char *TAG = "SD";

// Define SPI pins for SD card
#ifdef BOARD_SPARKFUN_ESP32_WROOM_C
#define SD_MISO_PIN GPIO_NUM_19
#define SD_MOSI_PIN GPIO_NUM_23
#define SD_CLK_PIN GPIO_NUM_18
#define SD_CS_PIN GPIO_NUM_5
#else
#error "Board not defined"
#endif

#define MOUNT_POINT "/sd"

static sdmmc_host_t host = SDSPI_HOST_DEFAULT();
static sdmmc_card_t *card = NULL;
static spi_bus_config_t bus_cfg = {
    .mosi_io_num = SD_MOSI_PIN,
    .miso_io_num = SD_MISO_PIN,
    .sclk_io_num = SD_CLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
};
static esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};

static bool is_mounted = false;

esp_err_t sdcard_init(void)
{
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        spi_bus_free(host.slot);
        return ret;
    }

    is_mounted = true;

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t sdcard_list_files(const char *path)
{
    if (!is_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, path);

    ESP_LOGI(TAG, "Listing directory: %s", full_path);

    DIR *dir = opendir(full_path);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", full_path);
        return ESP_FAIL;
    }

    struct dirent *entry;
    int file_count = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        file_count++;

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", full_path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) == 0)
        {
            char type_char = (st.st_mode & S_IFDIR) ? 'd' : '-';
            char size_str[32];

            if (st.st_mode & S_IFDIR)
            {
                sprintf(size_str, "<DIR>");
                ESP_LOGI(TAG, "%c %s/", type_char, entry->d_name);
            }
            else
            {
                sprintf(size_str, "%ld", st.st_size);
                ESP_LOGI(TAG, "%c %s (%s bytes)", type_char, entry->d_name, size_str);
            }
        }
    }

    closedir(dir);
    return ESP_OK;
}

esp_err_t sdcard_get_space(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (!is_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    FATFS *fs;
    DWORD free_clusters;
    size_t total_sectors;
    size_t free_sectors;
    size_t total_size;
    size_t free_size;

    f_getfree(MOUNT_POINT, &free_clusters, &fs);
    total_sectors = (fs->n_fatent - 2) * fs->csize;
    free_sectors = free_clusters * fs->csize;

    total_size = total_sectors * fs->ssize;
    free_size = free_sectors * fs->ssize;

    ESP_LOGI(TAG, "SD Card: Total: %llu bytes, Free: %llu bytes",
             (unsigned long long)total_size, (unsigned long long)free_size);

    if (total_bytes)
    {
        *total_bytes = total_size;
    }

    if (free_bytes)
    {
        *free_bytes = free_size;
    }

    return ESP_OK;
}

FILE *sdcard_open_file(const char *filepath, bool overwrite)
{
    if (!is_mounted)
    {
        ESP_LOGE(TAG, "SD card not mounted");
        return NULL;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, filepath);

    // Open file for writing in binary mode
    // "wb" - create new file or truncate existing (overwrite)
    // "ab" - open existing file for appending or create new
    const char *mode = overwrite ? "wb" : "ab";

    FILE *file = fopen(full_path, mode);
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        return NULL;
    }

    ESP_LOGI(TAG, "Opened file for writing: %s", full_path);
    return file;
}

esp_err_t sdcard_file_write(FILE *file, const void *data, size_t size)
{
    if (!file || !data)
    {
        ESP_LOGE(TAG, "Invalid file handle or data pointer");
        return ESP_ERR_INVALID_ARG;
    }

    size_t bytes_written = fwrite(data, 1, size, file);
    if (bytes_written != size)
    {
        ESP_LOGE(TAG, "Failed to write all data: %d of %d bytes written",
                 bytes_written, size);
        return ESP_FAIL;
    }

    // Flush the data to ensure it's written to disk
    if (fflush(file) != 0)
    {
        ESP_LOGE(TAG, "Error flushing file");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Successfully wrote %d bytes", size);
    return ESP_OK;
}

esp_err_t sdcard_close_file(FILE *file)
{
    if (!file)
    {
        ESP_LOGE(TAG, "Invalid file handle");
        return ESP_ERR_INVALID_ARG;
    }

    if (fclose(file) != 0)
    {
        ESP_LOGE(TAG, "Error closing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File closed successfully");
    return ESP_OK;
}
