# USB-DAC with interactive Parametric EQ

Raspberry-Pi Pico based USB DAC with parametric equalizer

* No soldering needed (if you use genuine Raspberry Pi Pico with pinheaders)
* Easy to operate with four buttons and large LCD display
* Setting can be saved into the flash memory (it is automatically recalled when it is powered on)

## Ver.2 : ready for room EQ compensation
  
![peq1](https://github.com/user-attachments/assets/26ee6bd6-c989-4141-9d0c-000d5f50ffac)
Green line shows the center frequency of EQ2. The frequency (250Hz) is shown on top as well.

* 4ch fully controllable IIR filters
* Precise tuning is possible for all parameters
  - Steps changed by one click of button is smaller than previous version
  - The values are shown numerically as well
* Rapid value change by press and hold the button
* Sample frequency is now displayed
  
![peq2](https://github.com/user-attachments/assets/83a00a3c-b2c8-41a1-8a6d-647b74b13bc5)
Sample frequency is shown on top. If the frequency changes, LCD is automatically turned on and shows it 

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
  - EQn FREQ
  - EQn GAIN
  - EQn WIDTH
  - SETTINGS (save settings to flash or restore factory settings)
* Top-Right and Bottom-Right buttons increase / decrease values (except SETTINGS mode)

## Limitations and tips

* When the settings are saved to flash memory, the sound card stops (the board reboots)
* High gain (high peak of EQ or high total gain) sometimes causes noise (overflow of 16bit values)
* If sound is playing back, the UI slows down (If sound stops, UI is sufficiently responsive)
* If you use VCC-GND YD-RP2040 board (compatibles of Raspberry-Pi Pico), bridge pin 39 and 40 (Vin and Vout) because Waveshare boards are powered from Pin39 (but pin 39 of YD-RP2040 can not supply power)
![boardss](https://github.com/user-attachments/assets/48f579f6-3e2b-4a8b-b544-320f3714dd84)

## Base softwares

* [usb-sound-card sample](https://github.com/raspberrypi/pico-playground/tree/master/apps/usb_sound_card) in pico-playground of pico-SDK
  - mute function and USB-audio rate feedback is implemented
* [Sample code](https://files.waveshare.com/wiki/Pico-1.3-LCD/RP2040-LCD-LVGL.zip) of [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (but I do not use LVGL functions)
* [bitmap-OSD-font](https://github.com/frisnit/bitmap-OSD-font/tree/master) (font.c)

## Other resources

* [Implementation of BiQuad digital filter in C](https://www.utsbox.com/?page_id=523)

## Example of room EQ compensation using REW

### Calculating parameter
![eq](https://github.com/user-attachments/assets/36b21219-ed9d-4f15-886f-ccbe9e356994)
* Brown curve shows the measured frequency response without EQ
* Blue curve shows the target frequency response
* Red curve shows simulated (predicted) frequency response if calculated EQ is applied
* "EQ filters" window shows calculated EQ parameters

### Result (measured frequency response with and without equalizer)
![result](https://github.com/user-attachments/assets/5c1578c2-36f0-4bf9-8b50-9b4f43e6ff2d)
* Red curve shows the measured frequency response without EQ
* Green curve shows measured frequency response using this USB-DAC with EQ

Please see that the measured curve matches predicted curve very well
