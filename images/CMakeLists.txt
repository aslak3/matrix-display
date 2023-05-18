add_library(
    images
    images.c
    weather-16x16.c
    weather-28x28.c
)

target_link_libraries(images pico_stdlib)

target_include_directories(images PUBLIC)
