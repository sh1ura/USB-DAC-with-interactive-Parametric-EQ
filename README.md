# USB-DAC with interactive Parametric EQ

Raspberry-Pi Pico based USB DAC with parametric equalizer

![DSC_2694_01ts](https://github.com/user-attachments/assets/085e5765-5baa-4c8d-9bde-daaae47f6dad)


* No soldering needed (if you use genuine Raspberry Pi Pico with pinheaders)
* Easy to operate with four buttons and large LCD display
* Setting can be saved into the flash memory (it is automatically recalled when it is powered on)
* Number of parametric equalizer can be changed (from 2 to 4)

## Necessary Parts

* Raspberry Pi Pico (RP2040) or its compatible board
* [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (2inch LCD Display Module)
* [Waveshare Pico-Audio](https://www.waveshare.com/wiki/Pico-Audio) (I used Rev.1 board with Burr-Brown PCM5101A)

## How to make (with precompiled binary)

1. Stack three boards
2. Connect USB to your PC while press and hold BOOTSEL button
3. Drag and drop soud-eq.uf2 file to the RPI-RP2 icon

Then this board will be recognized as "Pico Sound Card with EQ"

## Operation

* Top-Left button toggles EQ (EQ ON or THROUGH)
* Bottom-Left button (SEL) switches modes (n = number of parametric equalizer),
  - TOTAL GAIN
  - EQn GAIN
  - EQn FREQ
  - EQn WIDTH
  - SETTINGS (save settings to flash or restore factory settings)
* Top-Right and Bottom-Right buttons increase / decrease values (except SETTINGS mode)

Operation: https://www.youtube.com/watch?v=vReo5DAdtEo

## Limitations and tips

* When the settings are saved to flash memory, the sound card stops (the board reboots)
* High gain (high peak of EQ or high total gain) sometimes causes noise (overflow of 16bit values)
* If you use VCC-GND YD-RP2040 board (compatibles of Raspberry-Pi Pico), bridge pin 39 and 40 (Vin and Vout) because Waveshare boards are powered from Pin39 (but pin 39 of YD-RP2040 can not supply power)
![boardss](https://github.com/user-attachments/assets/48f579f6-3e2b-4a8b-b544-320f3714dd84)

## Base softwares

* [usb-sound-card sample](https://github.com/raspberrypi/pico-playground/tree/master/apps/usb_sound_card) in pico-playground of pico-SDK
  - mute function and USB-audio rate feedback is implemented
* [Sample code](https://files.waveshare.com/wiki/Pico-1.3-LCD/RP2040-LCD-LVGL.zip) of [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (but I do not use LVGL functions)
* [bitmap-OSD-font](https://github.com/frisnit/bitmap-OSD-font/tree/master) (font.c)

## Other resources

* [Implementation of BiQuad digital filter in C](https://www.utsbox.com/?page_id=523)
