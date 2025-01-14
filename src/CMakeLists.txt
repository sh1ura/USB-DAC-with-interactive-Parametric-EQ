cmake_minimum_required(VERSION 3.12)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(sound-eq C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(./lib/Config)
add_subdirectory(./lib/LCD)
# add_subdirectory(./lib/lvgl)
# add_subdirectory(./examples)

# include_directories(./examples)
include_directories(./lib/Config)
include_directories(./lib/LCD)
# include_directories(./lib/lvgl)
# include_directories(./examples/inc)

add_executable(sound-eq
	src/sound-eq.c
	src/eq.c
	src/draw.c
	)

target_compile_definitions(sound-eq PRIVATE
    AUDIO_FREQ_MAX=48000

    PICO_AUDIO_I2S_DATA_PIN=26
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=27

    # ours are zero based, so say so
    PICO_USBDEV_USE_ZERO_BASED_INTERFACES=1

    # need large descriptor
    PICO_USBDEV_MAX_DESCRIPTOR_SIZE=256

    PICO_USBDEV_ISOCHRONOUS_BUFFER_STRIDE_TYPE=1
    PICO_USBDEV_ENABLE_DEBUG_TRAgCE

    PICO_AUDIO_I2S_MONO_OUTPUT=0
    PICO_AUDIO_I2S_MONO_INPUT=0
)

pico_enable_stdio_uart(sound-eq 1)
#pico_enable_stdio_usb(sound-eq 0)

target_link_libraries(sound-eq
	pico_stdlib usb_device
	pico_audio_i2s pico_multicore
	LCD
#	LVGL
	Config
	pico_stdlib
	hardware_spi
	hardware_i2c
	hardware_dma
	hardware_flash
)

pico_add_extra_outputs(sound-eq)
