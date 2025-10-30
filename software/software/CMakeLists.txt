# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(main_software C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(main_software main_software.c inc/ssd1306.c inc/ssd1306_fonts.c inc/lora_RFM95.c)

pico_set_program_name(main_software "main_software")
pico_set_program_version(main_software "0.1")

pico_generate_pio_header(main_software ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(main_software 1)
pico_enable_stdio_usb(main_software 1)

# Add the standard library to the build
target_link_libraries(main_software
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(main_software PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/inc
)

# Add any user requested libraries
target_link_libraries(main_software 
        hardware_spi
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_interp
        hardware_timer
        hardware_watchdog
        hardware_clocks    
    )

pico_add_extra_outputs(main_software)

