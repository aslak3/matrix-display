cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(matrix CXX C ASM)
set(PROJECT matrix)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(PICO_BOARD pico_w)
set(SSID     ${WIFI_SSID})
set(PASSWORD ${WIFI_PASSWORD})

pico_sdk_init()

add_executable(
    matrix
    matrix.cpp
    mqtt.cpp
    framebuffer.cpp
    noto.c
    tinyseven.c
    cloudysun.c
    cloudysuntiny.c
#    ${FREERTOS_KERNEL_PATH}
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/port.c
)

target_include_directories(matrix PUBLIC
    .
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/include
    ${CMAKE_CURRENT_LIST_DIR}
)

# pull in common dependencies
target_link_libraries(matrix pico_stdlib hardware_i2c
#    pico_cyw43_arch_lwip_threadsafe_background
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_mqtt
    FreeRTOS-Kernel-Heap4
)

target_compile_definitions(${PROJECT} PRIVATE
    WIFI_SSID=\"${SSID}\"
    WIFI_PASSWORD=\"${PASSWORD}\"
)

pico_enable_stdio_usb(matrix 1)
pico_enable_stdio_uart(matrix 1)

pico_add_extra_outputs(matrix)
