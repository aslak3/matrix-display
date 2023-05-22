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

extern const image_dsc_t clear_night_32x32_dsc;
extern const image_dsc_t cloudy_32x32_dsc;
extern const image_dsc_t fog_32x32_dsc;
extern const image_dsc_t hail_32x32_dsc;
extern const image_dsc_t lightning_32x32_dsc;
extern const image_dsc_t lightning_rainy_32x32_dsc;
extern const image_dsc_t partlycloudy_32x32_dsc;
extern const image_dsc_t pouring_32x32_dsc;
extern const image_dsc_t rainy_32x32_dsc;
extern const image_dsc_t snowy_rainy_32x32_dsc;
extern const image_dsc_t snowy_32x32_dsc;
extern const image_dsc_t sunny_32x32_dsc;
extern const image_dsc_t windy_32x32_dsc;

extern const image_dsc_t mp_off_32x32_dsc;
extern const image_dsc_t mp_stop_32x32_dsc;
extern const image_dsc_t mp_pause_32x32_dsc;
extern const image_dsc_t mp_play_32x32_dsc;

extern const image_dsc_t porch_21x8_dsc;

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
    { "windy", &windy_32x32_dsc },
    
    { "clear-night", &clear_night_32x32_dsc },
    { "cloudy", &cloudy_32x32_dsc },
    { "fog", &fog_32x32_dsc },
    { "hail", &hail_32x32_dsc },
    { "lightning", &lightning_32x32_dsc },
    { "lightning-rainy", &lightning_rainy_32x32_dsc },
    { "partlycloudy", &partlycloudy_32x32_dsc },
    { "pouring", &pouring_32x32_dsc },
    { "rainy", &rainy_32x32_dsc },
    { "snowy-rainy", &snowy_rainy_32x32_dsc },
    { "snowy", &snowy_32x32_dsc },
    { "sunny", &sunny_32x32_dsc },
    { "windy", &windy_32x32_dsc },

    { "mp-off", &mp_off_32x32_dsc },
    { "mp-stop", &mp_stop_32x32_dsc },
    { "mp-pause", &mp_pause_32x32_dsc },
    { "mp-play", &mp_play_32x32_dsc },

    { "porch", &porch_21x8_dsc },

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
