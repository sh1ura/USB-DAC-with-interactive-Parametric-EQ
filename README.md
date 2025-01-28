# USB-DAC with interactive Parametric EQ

Raspberry-Pi Pico based USB DAC with parametric equalizer (PEQ)

* No soldering needed (if you use genuine Raspberry Pi Pico with pinheaders)
* Easy to operate with four buttons and large LCD display
* Setting can be saved into the flash memory (it is automatically recalled when it is powered on)
* Maximum number of PEQ channel is 8 (if you use RP2350. For RP2040, max. ch. is 4)

## Ver.2 : ready for room EQ compensation
  
![peq1](https://github.com/user-attachments/assets/61e4f219-03b9-48d3-873b-3ad1a90c01a4)
Green line shows the center frequency of EQ4. The frequency (2.8kHz) is shown on top as well.

* Fully controllable IIR (BiQuad) filters
* Precise tuning is possible for all parameters
  - Steps changed by one button click is smaller than previous version
  - The values are displayed numerically as well
* Rapid value change by press and hold the button
* Current sample frequency is now displayed
  
![settings](https://github.com/user-attachments/assets/a3da607c-3932-472d-88ca-f0312c5be137)
Sample frequency is shown on top. If the frequency changes, LCD is automatically turned on and shows it.
Parameters of the equalizer are summarized. You can save or reset parameters here.
The buffer occupancy is shown in real-time.

## Necessary Parts

* Raspberry Pi Pico (RP2040), Pico2 (RP2350) or compatible board. **RP2350 is recommended.**
* [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (2inch LCD Display Module)
* [Waveshare Pico-Audio](https://www.waveshare.com/wiki/Pico-Audio) (I used Rev.1 board with Burr-Brown PCM5101A)

## How to make (with precompiled binary)

1. Stack three boards
2. Connect USB to your PC while press and hold BOOTSEL button
3. Drag and drop soud-eq-RP????-?ch.uf2 file to the RPI-RP2 (or RP2350) icon

Then this board will be recognized as "Pico Sound Card with EQ"

## Operation

* Top-Left button toggles EQ (EQ/ON or OFF)
* Bottom-Left button (SEL) switches modes (n = number of parametric equalizer),
  - TOTAL GAIN
  - EQn FREQ
  - EQn GAIN
  - EQn WIDTH
  - SETTINGS (save settings to flash or restore factory settings)
* Top-Right and Bottom-Right buttons increase / decrease values (except SETTINGS mode)

Please see the movies at https://youtu.be/eDUhabNW9hQ for operation.

## Limitations and tips

* When the settings are saved to flash memory, the sound card stops (the board reboots immediately)
* High gain (high peak of EQ or high total gain) sometimes causes noise (overflow of 16bit values)
* RP2040 only : If sound is playing back, the UI slows down (If sound stops, UI is sufficiently responsive. If you use RP2350, the UI is always responsive)
* If you use VCC-GND YD-RP2040 board (compatibles of Raspberry-Pi Pico), bridge pin 39 and 40 (Vin and Vout) because Waveshare boards are powered from Pin39 (but pin 39 of YD-RP2040 can not supply power)
![boardss](https://github.com/user-attachments/assets/48f579f6-3e2b-4a8b-b544-320f3714dd84)

## Base softwares

* [usb-sound-card sample](https://github.com/raspberrypi/pico-playground/tree/master/apps/usb_sound_card) in pico-playground of pico-SDK
  - mute function and USB-audio rate feedback is implemented
* [Sample code](https://files.waveshare.com/wiki/Pico-1.3-LCD/RP2040-LCD-LVGL.zip) of [Waveshare Pico LCD 2](https://www.waveshare.com/wiki/Pico-LCD-2) (but I do not use LVGL functions)

## Other resources

* [Implementation of BiQuad digital filter in C](https://www.utsbox.com/?page_id=523)
* [VCR OSD Mono(Font used for UI)](https://www.dafont.com/vcr-osd-mono.font)

## Example of room EQ compensation using [REW](https://www.roomeqwizard.com/)

See [instructables](https://www.instructables.com/Room-Acoustic-Correction-by-DIY-Parametric-Equaliz/) as well.

### Calculating parameter

![eq](https://github.com/user-attachments/assets/22f8555b-bc11-4afb-b21e-cdbd09bfa8b6)

* Blue curve shows the measured frequency response without EQ
* Black curve shows the target response
* Red curve shows simulated (predicted) frequency response if calculated EQ is applied
* "EQ filters" window shows parameters of optimized EQ

### Result (measured frequency response with and without equalizer)

![result](https://github.com/user-attachments/assets/5dec26f6-4f2a-462e-95ac-48f80f886839)

* Blue curve shows the measured frequency response without EQ
* Red curve shows measured frequency response compensated by this USB-DAC with EQ

Please see that the measured curve matches predicted curve very well.
