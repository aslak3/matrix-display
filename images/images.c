#include "string.h"

#include "images.h"

extern const image_dsc_t clear_night_16x16_dsc;
extern const image_dsc_t cloudy_16x16_dsc;
extern const image_dsc_t fog_16x16_dsc;
extern const image_dsc_t hail_16x16_dsc;
extern const image_dsc_t lightning_16x16_dsc;
extern const image_dsc_t lightning_rainy_16x16_dsc;
extern const image_dsc_t partlycloudy_16x16_dsc;
extern const image_dsc_t pouring_16x16_dsc;
extern const image_dsc_t rainy_16x16_dsc;
extern const image_dsc_t snowy_rainy_16x16_dsc;
extern const image_dsc_t snowy_16x16_dsc;
extern const image_dsc_t sunny_16x16_dsc;
extern const image_dsc_t windy_16x16_dsc;

extern const image_dsc_t clear_night_28x28_dsc;
extern const image_dsc_t cloudy_28x28_dsc;
extern const image_dsc_t fog_28x28_dsc;
extern const image_dsc_t hail_28x28_dsc;
extern const image_dsc_t lightning_28x28_dsc;
extern const image_dsc_t lightning_rainy_28x28_dsc;
extern const image_dsc_t partlycloudy_28x28_dsc;
extern const image_dsc_t pouring_28x28_dsc;
extern const image_dsc_t rainy_28x28_dsc;
extern const image_dsc_t snowy_rainy_28x28_dsc;
extern const image_dsc_t snowy_28x28_dsc;
extern const image_dsc_t sunny_28x28_dsc;
extern const image_dsc_t windy_28x28_dsc;

extern const image_dsc_t mp_off_32x32_dsc;
extern const image_dsc_t mp_stop_32x32_dsc;
extern const image_dsc_t mp_pause_32x32_dsc;
extern const image_dsc_t mp_play_32x32_dsc;

static image_t images[] = {
    { "clear-night", &clear_night_16x16_dsc },
    { "cloudy", &cloudy_16x16_dsc },
    { "fog", &fog_16x16_dsc },
    { "hail", &hail_16x16_dsc },
    { "lightning", &lightning_16x16_dsc },
    { "lightning-rainy", &lightning_rainy_16x16_dsc },
    { "partlycloudy", &partlycloudy_16x16_dsc },
    { "pouring", &pouring_16x16_dsc },
    { "rainy", &rainy_16x16_dsc },
    { "snowy-rainy", &snowy_rainy_16x16_dsc },
    { "snowy", &snowy_16x16_dsc },
    { "sunny", &sunny_16x16_dsc },
    { "windy", &windy_28x28_dsc },
    
    { "clear-night", &clear_night_28x28_dsc },
    { "cloudy", &cloudy_28x28_dsc },
    { "fog", &fog_28x28_dsc },
    { "hail", &hail_28x28_dsc },
    { "lightning", &lightning_28x28_dsc },
    { "lightning-rainy", &lightning_rainy_28x28_dsc },
    { "partlycloudy", &partlycloudy_28x28_dsc },
    { "pouring", &pouring_28x28_dsc },
    { "rainy", &rainy_28x28_dsc },
    { "snowy-rainy", &snowy_rainy_28x28_dsc },
    { "snowy", &snowy_28x28_dsc },
    { "sunny", &sunny_28x28_dsc },
    { "windy", &windy_28x28_dsc },

    { "mp-off", &mp_off_32x32_dsc },
    { "mp-stop", &mp_stop_32x32_dsc },
    { "mp-pause", &mp_pause_32x32_dsc },
    { "mp-play", &mp_play_32x32_dsc },

    { "", NULL },
};

image_t *get_image(const char *name, int width, int height)
{
    for (image_t *i = images; i->image_dsc; i++) {
        if (strcmp(i->name, name) == 0) {
            if (i->image_dsc->width == width && i->image_dsc->height == height) {
                return i;
            }
        }
    }
    return NULL;
}
