cmake_minimum_required(VERSION 3.25)

include(local.cmake)
include(boards/${MATRIX_DISPLAY_BOARD}.cmake)

if(${MATRIX_DISPLAY_SDK} STREQUAL "PICO")
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
