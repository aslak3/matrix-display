set(MQTT_BROKER_PORT 1883)
include(../local.cmake)

set(SPI_TO_FPGA 0)
include(../boards/${MATRIX_DISPLAY_BOARD}.cmake)

include(../sensors.cmake)

set(PROJECT matrix-display)

if(${MATRIX_DISPLAY_SDK} STREQUAL "PICO")
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    project(matrix-display CXX C ASM)

    pico_sdk_init()

    set(SRC_DIR ${PROJECT_SOURCE_DIR})
elseif(${MATRIX_DISPLAY_SDK} STREQUAL "ESP32")
    set(SRC_DIR ${PROJECT_DIR}/src)
endif()

include(${SRC_DIR}/fonts/CMakeLists.txt)
include(${SRC_DIR}/images/CMakeLists.txt)

set(ALL_SOURCE
    main.cpp
    mqtt.cpp
    time.cpp
    sensor.cpp
    i2c.cpp
    framebuffer.cpp
    animation.cpp
    rtttl.cpp
    fonts/fonts.c
    images/images.c
    ${FONTS_SOURCE}
    ${IMAGES_SOURCE}
    cJSON.c
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${PROJECT_SOURCE_DIR}/=")

if(BME680_PRESENT)
    set(BH680_SOURCE sensor/bme680.cpp)
endif()
if(DS3231_PRESENT)
    set(DS3231_SOURCE sensor/ds3231.cpp)
endif()
if(BH1750_PRESENT)
    set(BH170_SOURCE sensor/bh1750.cpp)
endif()

set(SENSOR_SOURCE ${BH680_SOURCE} ${DS3231_SOURCE} ${BH170_SOURCE})

if(BUZZER_PRESENT)
    set(BUZZER_SOURCE buzzer.cpp)
endif()

if(DEVICE_PRETTY_NAME STREQUAL "")
    message(FATAL_ERROR "DEVICE_PRETTY_NAME must be set")
endif()
if(DEVICE_NAME STREQUAL "")
    message(FATAL_ERROR "DEVICE_NAME must be set")
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
    DEVICE_PRETTY_NAME="${DEVICE_PRETTY_NAME}"
    DEVICE_NAME="${DEVICE_NAME}"
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
    SPI_TO_FPGA=${SPI_TO_FPGA}
    BME680_PRESENT=${BME680_PRESENT}
    DS3231_PRESENT=${DS3231_PRESENT}
    BH1750_PRESENT=${BH1750_PRESENT}
    MQTT_BROKER_IP="${MQTT_BROKER_IP}"
    MQTT_BROKER_PORT=${MQTT_BROKER_PORT}
    MQTT_BROKER_USERNAME="${MQTT_BROKER_USERNAME}"
    MQTT_BROKER_PASSWORD="${MQTT_BROKER_PASSWORD}"
    # Override mqtt_opts.h setting
    MQTT_OUTPUT_RINGBUF_SIZE=2048
    MQTT_REQ_MAX_IN_FLIGHT=8
    # HUB75 pins (some may be not needed and missing, eg SPI to FPGA mode)
    HUB75_RED1_PIN=${HUB75_RED1_PIN}
    HUB75_GREEN1_PIN=${HUB75_GREEN1_PIN}
    HUB75_BLUE1_PIN=${HUB75_BLUE1_PIN}
    HUB75_RED2_PIN=${HUB75_RED2_PIN}
    HUB75_GREEN2_PIN=${HUB75_GREEN2_PIN}
    HUB75_BLUE2_PIN=${HUB75_BLUE2_PIN}
    HUB75_ROWSEL_A_PIN=${HUB75_ROWSEL_A_PIN}
    HUB75_ROWSEL_B_PIN=${HUB75_ROWSEL_B_PIN}
    HUB75_ROWSEL_C_PIN=${HUB75_ROWSEL_C_PIN}
    HUB75_ROWSEL_D_PIN=${HUB75_ROWSEL_D_PIN}
    HUB75_ROWSEL_E_PIN=${HUB75_ROWSEL_E_PIN}
    HUB75_CLK_PIN=${HUB75_CLK_PIN}
    HUB75_STROBE_PIN=${HUB75_STROBE_PIN}
    HUB75_OEN_PIN=${HUB75_OEN_PIN}
    # Other HUB75 things
    FRAME_DELAY_MS=${FRAME_DELAY_MS}
    FPGA_RESET_PIN=${FPGA_RESET_PIN}
    # I2C pins
    I2C_SDA_PIN=${I2C_SDA_PIN}
    I2C_SCL_PIN=${I2C_SCL_PIN}
)

if(${MATRIX_DISPLAY_SDK} STREQUAL "PICO")
    set(PICO_SDK 1)
    set(ESP32_SDK 0)

    set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})

    add_executable(
        ${PROJECT}
        ${ALL_SOURCE}
        ${SENSOR_SOURCE}
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
elseif(${MATRIX_DISPLAY_SDK} STREQUAL "ESP32")
    set(PICO_SDK 0)
    set(ESP32_SDK 1)

    set(MQTT_SOURCE "$ENV{IDF_PATH}/components/lwip/lwip/src/apps/mqtt/mqtt.c")

    idf_component_register(
        SRCS ${ALL_SOURCE} ${MQTT_SOURCE} ${SENSOR_SOURCE} ${BUZZER_SOURCE}
        REQUIRES esp_wifi esp_driver_gpio driver nvs_flash
    )

    target_include_directories(${COMPONENT_LIB} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
    add_compile_definitions(
        ${COMPILE_DEFS}
        PICO_SDK=${PICO_SDK}
        ESP32_SDK=${ESP32_SDK}
    )
endif()
