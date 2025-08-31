add_library(
    images
    images.c
    weather-16x16.c
    weather-32x32.c
    media_player.c
    misc.c
)

target_include_directories(images PUBLIC)
