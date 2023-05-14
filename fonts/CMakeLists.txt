add_library(
    fonts
    fonts.c
    noto10p.c
    noto16p.c
    noto20p.c
    IBM_PC_V1_8x8.c
)

target_link_libraries(fonts pico_stdlib)

target_include_directories(fonts PUBLIC)