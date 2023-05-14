#include "string.h"

#include "images.h"

extern const image_dsc_t clear_night_dsc;
extern const image_dsc_t fog_dsc;
extern const image_dsc_t hail_dsc;
extern const image_dsc_t lightning_dsc;
extern const image_dsc_t lightning_rainy_dsc;
extern const image_dsc_t partlycloudy_dsc;
extern const image_dsc_t pouring_dsc;
extern const image_dsc_t rainy_dsc;
extern const image_dsc_t snowy_rainy_dsc;
extern const image_dsc_t snowy_dsc;
extern const image_dsc_t sunny_dsc;
extern const image_dsc_t windy_dsc;

static image_t images[] = {
    { "clear-night", &clear_night_dsc },
    { "fog", &fog_dsc },
    { "hail", &hail_dsc },
    { "lightning", &lightning_dsc },
    { "lightning-rainy", &lightning_rainy_dsc },
    { "partlycloudy", &partlycloudy_dsc },
    { "pouring", &pouring_dsc },
    { "rainy", &rainy_dsc },
    { "snowy-rainy", &snowy_rainy_dsc },
    { "snowy", &snowy_dsc },
    { "sunny", &sunny_dsc },
    { "windy", &windy_dsc },
    { "", NULL },
};

image_t *get_image(const char *name)
{
    for (image_t *i = images; i->image_dsc; i++) {
        if (strcmp(i->name, name) == 0) {
            return i;
        }
    }
    return NULL;
}
