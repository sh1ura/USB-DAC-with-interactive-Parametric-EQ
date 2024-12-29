# source codes

## You need to build

* install SDK of Raspberry Pi Pico (pico-sdk, pico-extras, pico-examples, pico-playground)
* copy pico_extras_import.cmake and pico_sdk_import.cmake at base folder
* copy files in "lufa" folder at pico-playground/apps/usb_sound_card
* place all files of RP2040-LCD-LVGL

![files](https://github.com/user-attachments/assets/1963072a-2002-4da3-898a-7093794df11e)

## How to build

1. in build folder, run "cmake .."
2. in build folder, run "make"
