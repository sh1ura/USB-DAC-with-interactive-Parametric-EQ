# source codes

## You need to build

* install SDK of Raspberry Pi Pico (pico-sdk, pico-extras, pico-examples, pico-playground)
    - set path to pico-sdk by `export PICO_SDK_PATH=<path to pico-sdk>`
* copy pico_extras_import.cmake and pico_sdk_import.cmake at base folder
* copy files in "lufa" folder at pico-playground/apps/usb_sound_card
* copy contents of "lib" in [RP2040-LCD-LVGL](https://files.waveshare.com/wiki/Pico-1.3-LCD/RP2040-LCD-LVGL.zip)
* make build folder by `mkdir build`

File arrangement before build is as follows

![スクリーンショット 2025-01-14 12 10 31](https://github.com/user-attachments/assets/398ebdf0-1c07-414e-9961-01bf1674d80e)

## How to build

1. **in build folder**, run cmake as
   - if you use RP2040 (Rapsberry Pi Pico), run "cmake .."
   - if you use RP2350 (Raspberry Pi Pico 2), run "cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 .."
2. **in build folder**in build folder, run "make"
