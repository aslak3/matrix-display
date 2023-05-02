#include "pico/stdlib.h"

#define FB_WIDTH 64
#define FB_HEIGHT 32

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t padding;
} rgb_t;

class framebuffer {
    public:
        framebuffer(void);
        inline void swap(void);
        void clear(void);
        void point(int x, int y, rgb_t rgb);
        void hollowbox(int x, int y, int width, int height, rgb_t rgb);
        void filledbox(int x, int y, int width, int height, rgb_t rgb);
        int printchar(int x, int y, char c, rgb_t rgb);
        void printstring(int x, int y, const char *s, rgb_t rgb);
        void showimage(void);

        rgb_t fb_rgb1[FB_WIDTH][FB_HEIGHT];
        rgb_t fb_rgb2[FB_WIDTH][FB_HEIGHT];

        rgb_t (*foreground_rgb)[FB_WIDTH][FB_HEIGHT];
        rgb_t (*background_rgb)[FB_WIDTH][FB_HEIGHT];
};