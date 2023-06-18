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
