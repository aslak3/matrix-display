set(PROJECT matrix-display)

if($ENV{MATRIX_DISPLAY_SDK} STREQUAL "PICO")
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    set(PICO_BOARD pico2_w)

    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    project(matrix-display CXX C ASM)

    pico_sdk_init()

    set(SRC_DIR ${PROJECT_SOURCE_DIR})
else()
    set(SRC_DIR ${PROJECT_DIR}/src)
endif()

include(${SRC_DIR}/fonts/CMakeLists.txt)
include(${SRC_DIR}/images/CMakeLists.txt)

set(ALL_SOURCE
    main.cpp
    mqtt.cpp
    time.cpp
    framebuffer.cpp
    animation.cpp
    rtttl.cpp
    cJSON.c
    fonts/fonts.c
    images/images.c
    ${FONTS_SOURCE}
    ${IMAGES_SOURCE}
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${PROJECT_SOURCE_DIR}/=")

option(SPI_TO_FPGA "Use the iCE40 FPGA for the display" 1)
option(BME680_PRESENT "Use a BME680 for temperature, humidity and air preasure" 0)
set(WIFI_SSID "" CACHE STRING "Wi-Fi SSD network name")
set(WIFI_PASSWORD "" CACHE STRING "Wi-Fi password")
set(MQTT_BROKER_IP "" CACHE STRING "MQTT broker IP address")
set(MQTT_BROKER_PORT "1883" CACHE STRING "MQTT broker port number")
set(MQTT_BROKER_USERNAME "" CACHE STRING "MQTT broker username")
set(MQTT_BROKER_PASSWORD "" CACHE STRING "MQTT broker password")

include(${SRC_DIR}/../private.cmake)

if(BME680_PRESENT)
    set(CLIMATE_SOURCE bme680-climate.cpp)
elseif(DS3231_PRESENT)
    set(CLIMATE_SOURCE ds3231-climate.cpp)
else()
    set(CLIMAGE_SOURCE)
endif()

if(BUZZER_PRESENT)
    set(BUZZER_SOURCE buzzer.cpp)
endif()

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

set(COMPILE_DEFS
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
    SPI_TO_FPGA=${SPI_TO_FPGA}
    BME680_PRESENT=${BME680_PRESENT}
    MQTT_BROKER_IP="${MQTT_BROKER_IP}"
    MQTT_BROKER_PORT=${MQTT_BROKER_PORT}
    MQTT_BROKER_USERNAME="${MQTT_BROKER_USERNAME}"
    MQTT_BROKER_PASSWORD="${MQTT_BROKER_PASSWORD}"
)

if($ENV{MATRIX_DISPLAY_SDK} STREQUAL "PICO")
    set(PICO_SDK 1)
    set(ESP32_SDK 0)

    set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})

    add_executable(
        ${PROJECT}
        ${ALL_SOURCE}
        ${CLIMATE_SOURCE}
        ${BUZZER_SOURCE}
    )

    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

    target_include_directories(${PROJECT} PUBLIC "pico_include/" ${CMAKE_CURRENT_LIST_DIR})
    add_compile_definitions(
        ${COMPILE_DEFS}
        PICO_SDK=${PICO_SDK}
        ESP32_SDK=${ESP32_SDK}
        SNTP_SET_SYSTEM_TIME=set_system_time
    )
    target_link_libraries(${PROJECT} PUBLIC
        pico_stdlib hardware_i2c hardware_spi hardware_dma hardware_pwm
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mqtt pico_lwip_sntp
        FreeRTOS-Kernel-Heap4
    )

    pico_enable_stdio_usb(${PROJECT} 1)
    pico_enable_stdio_uart(${PROJECT} 1)

    pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/hub75.pio)

    pico_add_extra_outputs(${PROJECT})
else()
    set(PICO_SDK 0)
    set(ESP32_SDK 1)

    set(MQTT_SOURCE "$ENV{IDF_PATH}/components/lwip/lwip/src/apps/mqtt/mqtt.c")

    idf_component_register(
        SRCS ${ALL_SOURCE} ${MQTT_SOURCE} ${CLIMATE_SOURCE} ${BUZZER_SOURCE}
        REQUIRES esp_wifi esp_driver_gpio nvs_flash
    )

    target_include_directories(${COMPONENT_LIB} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
    add_compile_definitions(
        ${COMPILE_DEFS}
        PICO_SDK=${PICO_SDK}
        ESP32_SDK=${ESP32_SDK}
    )
endif()
