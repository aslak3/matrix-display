#include <pico/stdlib.h>

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

typedef union {
    rgb_t rgb[FB_HEIGHT][FB_WIDTH];
    uint32_t uint32[FB_HEIGHT][FB_WIDTH];
} fb_t;

class framebuffer {
    public:
        framebuffer(void);
        void clear(rgb_t rgb);
        void point(int x, int y, rgb_t rgb);
        void hollow_box(int x, int y, int width, int height, rgb_t rgb);
        void filled_box(int x, int y, int width, int height, rgb_t rgb);
        void shadow_box(int x, int y, int width, int height, uint8_t gamma);
        int print_char(font_t *font, int x, int y, char c, rgb_t rgb, bool length_only);
        void print_string(font_t *font, int x, int y, const char *s, rgb_t rgb);
        int string_length(font_t *font, const char *s);
        int print_wrapped_string(font_t *font, int y, const char *s, rgb_t rgb);
        void show_image(image_t *image, int x, int y);
        void show_image(image_t *image, int x, int y, uint8_t gamma, bool transparent);
        void set_brightness(uint8_t b);
        void atomic_back_to_fore_copy(void);
        void atomic_fore_copy_out(fb_t *out);

    private:
        uint8_t brightness;
        void set_pixel(int x, int y, rgb_t rgb);
        void set_pixel(int x, int y, rgb_t rgb, uint8_t gamma);
        rgb_t get_pixel(int x, int y);

        fb_t foreground;
        fb_t background;
};