# ESP32 GNSS Base Station

GNSS Base Station using ESP32 for RTK position services.

## Development Tool

* IDE: Visual Studio Code + PlatformIO
* Platform: Espressif 32
* Framework: Espressif IDF

Refer to file `platformio.ini` for detail settings.

PACKAGES:

``` sh
 - framework-espidf @ 3.50002.230601 (5.0.2) 
 - tool-cmake @ 3.16.4 
 - tool-esptoolpy @ 1.40501.0 (4.5.1) 
 - tool-idf @ 1.0.1 
 - tool-mconf @ 1.4060000.20190628 (406.0.0) 
 - tool-ninja @ 1.9.0 
 - toolchain-esp32ulp @ 1.23500.220830 (2.35.0) 
 - toolchain-xtensa-esp32 @ 11.2.0+2022r1
```

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

* "Fix" HTTP Server slow respone:

    * Call `esp_wifi_set_ps(WIFI_PS_NONE)` to not use Power Save mode
    * In HTTP Server config, set `config.lru_purge_enable = true` to remove least recent used sockets.

* Async HTTP Server

    * Set `httpd_config_t config.close_fn = custom_httpd_close_func;`
    * In `custom_httpd_close_func`, do not close the socket
    * Use `httpd_socket_send()` to send data to the socket

* Speed Optimization

    * Build Mode: `Release`

    * Compiler Optimization

        * Goto `Menuconfig` > `Compilter options`\
          Set `Optimization Level` to `Optimize for performance (-O2)`

    * Disable logs from ESP:

        * Disable ROM bootloader output:\
            Pull down `GPIO15`.

        * Disable 2nd stage bootloader output:\
            Goto `Menuconfig` > `Bootloader config`\
            Set `Bootloader log verbosity` to `No output`

        * Only show Application error output:\
            Goto `Menuconfig` > `Component config` > `Log output`\
            Set `Default log verbosity` to `Error`

          or Disable Application output:\
            Goto `Menuconfig` > `Component config` > `Log output`\
            Set `Default log verbosity` to `No output`

        * Remove colors in log
            Goto `Menuconfig` > `Component config` > `Log output`\
            Deselect `Use ANSI terminal colors in log output`

    * WiFi Params

        * Goto `Menuconfig` > `Component config` > `WiFi`\
          Set `Max Wifi Static RX buffer` to `24` _(was `10`)_\
          Set `Max Wifi Dynamic RX buffer` to `64` _(was `32`)_\,
          Set `Max Wifi Dynamic TX buffer` to `64` _(was `32`)_\
          Set `WiFi AMPDU TX BA window size` to `32` _(was `6`)_\
          Set `WiFi AMPDU RX BA window size` to `32` _(was `6`)_\

        * Goto `Menuconfig` > `Component config` > `LWIP` > `TCP`\
          Set `Default send buffer size` to `65535` (64K) _(was `5744`)_\
          Set `Default receive window size` to `65535` _(was `5744`)_\
