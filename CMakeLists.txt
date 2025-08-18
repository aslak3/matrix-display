cmake_minimum_required(VERSION 3.25)

set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})
set(PICO_BOARD pico_w)

include(private.cmake)

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(matrix-display CXX C ASM)
set(PROJECT matrix-display)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

option(SPI_TO_FPGA "Use the iCE40 FPGA for the display" 1)
option(BME680_PRESENT "Use a BME680 for temperature, humidity and air preasure" 0)
set(WIFI_SSID "" CACHE STRING "Wi-Fi SSD network name")
set(WIFI_PASSWORD "" CACHE STRING "Wi-Fi password")
set(MQTT_BROKER_IP "" CACHE STRING "MQTT broker IP address")
set(MQTT_BROKER_PORT "1883" CACHE STRING "MQTT broker port number")
set(MQTT_BROKER_USERNAME "" CACHE STRING "MQTT broker username")
set(MQTT_BROKER_PASSWORD "" CACHE STRING "MQTT broker password")

if(WIFI_SSID STREQUAL "")
    message(FATAL_ERROR "WIFI_SSID must be set")
endif()
if(WIFI_PASSWORD STREQUAL "")
    message(FATAL_ERROR "WIFI_PASSWORD must be set")
endif()

if(MQTT_BROKER_IP STREQUAL "")
    message(FATAL_ERROR "MQTT_BROKER_IP must be set[${WIFI_PASSWORD}]")
endif()
if(MQTT_BROKER_USERNAME STREQUAL "")
    message(FATAL_ERROR "MQTT_BROKER_USERNAME must be set[${WIFI_PASSWORD}]")
endif()
if(MQTT_BROKER_PASSWORD STREQUAL "")
    message(FATAL_ERROR "MQTT_BROKER_PASSWORD must be set[${WIFI_PASSWORD}]")
endif()

pico_sdk_init()

if(BME680_PRESENT)
    set(CLIMATE_SOURCE bme680-climate.cpp)
else()
    set(CLIMATE_SOURCE ds3231-climate.cpp)
endif()

add_executable(
    ${PROJECT}
    main.cpp
    mqtt.cpp
    i2c.cpp
    buzzer.cpp
    framebuffer.cpp
    animation.cpp
    rtttl.cpp
    cJSON.c
    ${CLIMATE_SOURCE}
)

add_subdirectory(
    fonts
)

add_subdirectory(
    images
)

target_include_directories(${PROJECT} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# pull in common dependencies
target_link_libraries(${PROJECT} fonts images
    pico_stdlib hardware_i2c hardware_spi hardware_dma hardware_pwm
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_mqtt
    FreeRTOS-Kernel-Heap4
)

target_compile_definitions(${PROJECT} PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    SPI_TO_FPGA=${SPI_TO_FPGA}
    BME680_PRESENT=${BME680_PRESENT}
    MQTT_BROKER_IP=\"${MQTT_BROKER_IP}\"
    MQTT_BROKER_PORT=${MQTT_BROKER_PORT}
    MQTT_BROKER_USERNAME=\"${MQTT_BROKER_USERNAME}\"
    MQTT_BROKER_PASSWORD=\"${MQTT_BROKER_PASSWORD}\"
)

pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 1)

pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/hub75.pio)

pico_add_extra_outputs(${PROJECT})
