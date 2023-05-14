add_library(
    images
    images.c
    clear_night.c
    cloudy.c
    fog_alt.c
    fog.c
    hail.c
    images.c
    lightning.c
    lightning_rainy.c
    partlycloudy.c
    pouring.c
    rain_alt.c
    rainy.c
    snowy.c
    snowy_rainy.c
    sunny_alt.c
    sunny.c
    windy.c
)

target_link_libraries(images pico_stdlib)

target_include_directories(images PUBLIC)
