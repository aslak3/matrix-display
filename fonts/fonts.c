#include "string.h"

#include "fonts.h"

extern const lv_font_glyph_dsc_t noto10p_glyph_dsc[];
extern const uint8_t noto10p_glyph_bitmap[];
extern const lv_font_glyph_dsc_t noto16p_glyph_dsc[];
extern const uint8_t noto16p_glyph_bitmap[];
extern const lv_font_glyph_dsc_t noto20p_glyph_dsc[];
extern const uint8_t noto20p_glyph_bitmap[];

static font_t fonts[] = {
    { "Noto", 10, noto10p_glyph_dsc, noto10p_glyph_bitmap },
    { "Noto", 16, noto16p_glyph_dsc, noto16p_glyph_bitmap },
    { "Noto", 20, noto20p_glyph_dsc, noto20p_glyph_bitmap },
    { "", 0, NULL, NULL },
};

font_t *get_font(const char *name, int height)
{
    for (font_t *f = fonts; f->data; f++) {
        if (strcmp(f->name, name) == 0) {
            if (f->height == height) {
                return f;
            }
        }
    }
    return NULL;
}
