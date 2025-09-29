#include "matrix_display.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct lv_font_glyph_dsc {
    uint32_t w_px;
    uint32_t glyph_index;
} lv_font_glyph_dsc_t;

// If font_glyph_dsc is NULL, data is a simple uint8_t 1bpp array

typedef struct {
    char name[32];
    uint8_t height;
    uint8_t fixed_width;
    const lv_font_glyph_dsc_t *lv_font_glyph_dsc; // Optional
    const uint8_t *data;
} font_t;

font_t *get_font(const char *name, int height);
void special_replacement(char *haystack, const char *needle, char replacement);

#ifdef __cplusplus
}
#endif
