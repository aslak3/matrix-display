#include "pico/stdlib.h"

#include "fonts/fonts.h"
#include "images/images.h"

#define FB_WIDTH 64
#define FB_HEIGHT 32

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t dummy;
} rgb_t;

class framebuffer {
    public:
        framebuffer(void);
        void clear(rgb_t rgb);
        void point(int x, int y, rgb_t rgb);
        void hollowbox(int x, int y, int width, int height, rgb_t rgb);
        void filledbox(int x, int y, int width, int height, rgb_t rgb);
        int printchar(font_t *font, int x, int y, char c, rgb_t rgb, bool length_only);
        void printstring(font_t *font, int x, int y, const char *s, rgb_t rgb);
        int stringlength(font_t *font, const char *s);
        void showimage(image_t *image, int x, int y);
        void atomic_back_to_fore_copy(void);
        void atomic_fore_copy_out(rgb_t *out);

    private:
        void set_pixel(int x, int y, rgb_t rgb);

        rgb_t foreground_rgb[FB_HEIGHT][FB_WIDTH];
        rgb_t background_rgb[FB_HEIGHT][FB_WIDTH];
};