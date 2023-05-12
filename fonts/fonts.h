#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct lv_font_glyph_dsc {
    uint32_t w_px;
    uint32_t glyph_index;
} lv_font_glyph_dsc_t;

typedef struct {
    char name[32];
    uint8_t height;
    const lv_font_glyph_dsc_t *lv_font_glyph_dsc;
    const uint8_t *data;
} font_t;

font_t *get_font(const char *name, int height);

#ifdef __cplusplus
}
#endif
