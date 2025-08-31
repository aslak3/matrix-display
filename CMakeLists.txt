cmake_minimum_required(VERSION 3.25)

if($ENV{MATRIX_DISPLAY_SDK} STREQUAL "PICO")
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    set(PICO_BOARD pico_w)

    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    project(matrix-display CXX C ASM)

    pico_sdk_init()

    add_subdirectory(src)
else()
    set(IDF_TARGET esp32s3)
    set(EXTRA_COMPONENT_DIRS src)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    # MINIMAL_BUILD ON doesn't work if the main component is not called
    # main. Yay.
    set(COMPONENTS src ${TEST_COMPONENTS})
    set(minimal_build ON)
    project(matrix-display)
endif()
