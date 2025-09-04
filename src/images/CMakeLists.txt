set(IMAGES_SOURCE
    images.c
    weather-16x16.c
    weather-32x32.c
    media_player.c
    misc.c
)

list(TRANSFORM IMAGES_SOURCE PREPEND "images/")
