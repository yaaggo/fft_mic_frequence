cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(fft_frequence_detector C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

file(GLOB INCLUDE "include/*.c")
file(GLOB SRC "src/*.c")

add_executable(fft_frequence_detector src/main.c src/fft_analyzer.c ${INCLUDE})

pico_set_program_name(fft_frequence_detector "fft_frequence_detector")
pico_set_program_version(fft_frequence_detector "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(fft_frequence_detector 1)
pico_enable_stdio_usb(fft_frequence_detector 1)

# Add the standard library to the build
target_link_libraries(fft_frequence_detector
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(fft_frequence_detector PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(fft_frequence_detector 
        hardware_clocks
        hardware_gpio
        hardware_pwm
        hardware_i2c
        hardware_adc
        )

pico_add_extra_outputs(fft_frequence_detector)