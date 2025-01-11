# source codes

## You need to build

* install SDK of Raspberry Pi Pico (pico-sdk, pico-extras, pico-examples, pico-playground)
* copy pico_extras_import.cmake and pico_sdk_import.cmake at base folder
* copy files in "lufa" folder at pico-playground/apps/usb_sound_card
* copy contents of "lib" and "examples" in [RP2040-LCD-LVGL](https://files.waveshare.com/wiki/Pico-1.3-LCD/RP2040-LCD-LVGL.zip) 

![files](https://github.com/user-attachments/assets/1963072a-2002-4da3-898a-7093794df11e)

## How to build

1. make build folder by "mkdir build"
2. **in build folder**, run cmake as
   - if you use RP2040 (Rapsberry Pi Pico), run "cmake .."
   - if you use RP2350 (Raspberry Pi Pico 2), run "cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 .."
3. **in build folder**in build folder, run "make"
