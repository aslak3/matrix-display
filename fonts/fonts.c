#include "string.h"

#include "fonts.h"

extern const lv_font_glyph_dsc_t noto10p_glyph_dsc[];
extern const uint8_t noto10p_glyph_bitmap[];
extern const lv_font_glyph_dsc_t noto16p_glyph_dsc[];
extern const uint8_t noto16p_glyph_bitmap[];
extern const lv_font_glyph_dsc_t noto20p_glyph_dsc[];
extern const uint8_t noto20p_glyph_bitmap[];
extern const uint8_t IBM_PC_V1_8x8[];
extern const uint8_t tiny_5x6[];

static font_t fonts[] = {
    { "Noto", 10, 0, noto10p_glyph_dsc, noto10p_glyph_bitmap },
    { "Noto", 16, 0, noto16p_glyph_dsc, noto16p_glyph_bitmap },
    { "Noto", 20, 0, noto20p_glyph_dsc, noto20p_glyph_bitmap },
    { "IBM", 8, 8, NULL, IBM_PC_V1_8x8 },
    { "tiny", 6, 0, NULL, tiny_5x6 }, // Proportional 1bpp
    { "", 0, 0, NULL, NULL },
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
