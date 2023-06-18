# ESP32 GNSS Base Station

GNSS Base Station using ESP32 for RTK position services.

## Development Tool

* IDE: Visual Studio Code + PlatformIO
* Platform: Espressif 32
* Framework: Espressif IDF

Refer to file `platformio.ini` for detail settings.

## Target

This firmware is developed for [Espressif ESP32 DevKit C](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html), with below specifications:

* CPU: ESP32-WROOM-32D (ESP32-D0WDQ6), 2-core MIPS Xtensa®32-bit LX6 MCUs @ 240 MHZ
* ROM: 448 KB
* SRAM: 320 KB
* Flash: 4 MB
* WiFi: STA/AP/STA+AP
* Bluetooth: 4.2 + BLE
* No on-board debugger

However, this firmware can be built for other EPS32 product lines with few changes in eps32 SDK Configs.

## Platform IO config

* Extra settings match to the ESP Menuconfig items:

    ``` ini
    board_build.f_flash = 80000000L
    board_build.f_cpu = 240000000L
    board_build.flash_mode = qio
    board_build.partitions = partitions.csv
    board_upload.flash_size = 4MB
    build_type = debug
    monitor_speed = 115200
    monitor_filters = esp32_exception_decoder
    ```

## ESP32 Menuconfig

* Serial Flasher Config:
    * Flash SPI Mode: QIO
    * Flash SPI Speed: 80 Mhz
    * Flash size: 4 MB
    * Detect flash size

* Partition Table:
    * Partition Table: custom
    * Custom Partition CSV file: partitions.csv

* Component config:
    * HTTP Server:
        * Max HTTP Request Header Length: 1024
    * ESP System Settings:
        * CPU Frequency: 240 MHz
        * Panic handler behaviour: Print registers and halt

## Firmware notes

* Do not use _Document Type Declaration_, it causes rendering problem (I still don't know why):

    ``` html
    <!doctype html> <==  remove this
    <html lang="en">
    ```

* Disable logs from ESP:

    * Disable ROM bootloader output:\
        Pull down `GPIO15`.

    * Disable 2nd stage bootloader output:\
        Goto `Menuconfig` > `Bootloader config`\
        Set `Bootloader log verbosity` to `No output`

    * Disable Application output:\
        Goto `Menuconfig` > `Component config` > `Log output`\
        Set `Default log verbosity` to `No output`

* "Fix" HTTP Server slow respone:

    * Call `esp_wifi_set_ps(WIFI_PS_NONE)` to not use Power Save mode
    * In HTTP Server config, set `config.lru_purge_enable = true` to remove least recent used sockets.
