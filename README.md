# USB-DAC-with-interactive-Parametric-EQ
Raspberry-Pi Pico based USB DAC with 2ch parametric equalizer

![DSC_2690ts](https://github.com/user-attachments/assets/6cd02f6a-7b1e-4e5e-8dca-a2b30f005c9e)

* No soldering needed (if you use genuine Raspberry Pi Pico with pinheaders)
* Easy to operate with four buttons and large LCD display
* Setting can be saved into the flash memory (it is automatically recalled when it is powered on)

## Necessary Parts

* Raspberry Pi Pico (RP2040) or its compatible board
* [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (2inch LCD Display Module)
* [Waveshare Pico-Audio](https://www.waveshare.com/wiki/Pico-Audio) (I used Rev.1 board with Burr-Brown PCM5101A)

## How to make

1. Stack three boards
2. Connect USB to your PC while press and hold BOOTSEL button
3. Drag and drop soud-eq.uf2 file to the RPI-RP2 icon

Then this board will be recognized as "Pico Sound Card with EQ"

## Limitations and tips

* When the settings are saved to flash memory, the sound card stops (the board reboots)
* If you use VCC-GND YD-RP2040 board (compatibles of Raspberry-Pi Pico), bridge pin 39 and 40 (Vin and Vout) because Waveshare boards are powered from Pin39 (but pin 39 of YD-RP2040 can not supply power)

## Base softwares

* [usb-sound-card sample](https://github.com/raspberrypi/pico-playground/tree/master/apps/usb_sound_card) in pico-playground of pico-SDK
* [Sample code](https://files.waveshare.com/wiki/Pico-1.3-LCD/RP2040-LCD-LVGL.zip) of [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (but I do not use LVGL functions)
* [bitmap-OSD-font](https://github.com/frisnit/bitmap-OSD-font/tree/master) (font.c)

## Other resources

* [Implementation of BiQuad digital filter in C](https://www.utsbox.com/?page_id=523)
* 
