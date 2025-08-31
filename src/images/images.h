#include "../matrix_display.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    uint32_t width;
    uint32_t height;
    const uint8_t *data;
} image_dsc_t;

typedef struct {
    char name[32];
    const image_dsc_t *image_dsc;
} image_t;


image_t *get_image(const char *name, int width, int height);

#ifdef __cplusplus
}
#endif
