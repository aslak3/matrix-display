#include <stdio.h>

#include "pico/stdlib.h"

#include "framebuffer.hpp"
#include "noto.h"

framebuffer::framebuffer(void) {
    foreground_rgb = &fb_rgb1;

    background_rgb = &fb_rgb2;
}

void framebuffer::swap(void) {
    rgb_t (*temp)[FB_WIDTH][FB_HEIGHT];

    temp = background_rgb;
    background_rgb = foreground_rgb;
    foreground_rgb = temp;
}

void framebuffer::clear(void) {
    for (int r = 0; r < FB_HEIGHT; r++) {
        for (int c = 0; c < FB_WIDTH; c++) {
            (*background_rgb)[c][r] = (rgb_t) {};
        }
    }
}

void framebuffer::point(int x, int y, rgb_t rgb)
{
    (*background_rgb)[x][y] = rgb;
}

void framebuffer::hollowbox(int x, int y, int width, int height, rgb_t rgb)
{
    for (int p = x; p < x + width; p++) {
        (*background_rgb)[p][y] = rgb;
        (*background_rgb)[p][y + height - 1] = rgb;
    }

    for (int p = y; p < y + height; p++) {
        (*background_rgb)[x][p] = rgb;
        (*background_rgb)[x + width - 1][p] = rgb;
    }
}

void framebuffer::filledbox(int x, int y, int width, int height, rgb_t rgb)
{
    for (int r = y; r < y + height; r++) {
        for (int c = x; c < x + width; c++) {
            (*background_rgb)[c][r] = rgb;
        }
    }
}

extern const lv_font_glyph_dsc_t noto_glyph_dsc[];
extern const uint8_t noto_glyph_bitmap[];

int framebuffer::printchar(int x, int y, char c, rgb_t rgb)
{
    int index = (int)(c - ' ');
    int count = noto_glyph_dsc[index].glyph_index;
    int width = noto_glyph_dsc[index].w_px;

    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < width; c++) {
            if (noto_glyph_bitmap[count] > 0x20 && x + c < FB_WIDTH && x + c >= 0) {
                (*background_rgb)[x + c][y + 16 - r] = (rgb_t) {
                    .red =   (uint8_t)((uint32_t)(noto_glyph_bitmap[count] * rgb.red) / 256),
                    .green = (uint8_t)((uint32_t)(noto_glyph_bitmap[count] * rgb.green) / 256),
                    .blue =  (uint8_t)((uint32_t)(noto_glyph_bitmap[count] * rgb.blue) / 256),
                };
            }
            count++;
        }
    }
    return width;
}

void framebuffer::printstring(int x, int y, const char *s, rgb_t rgb)
{
    int running_x = x;
    for (; *s; s++) {
        running_x += 1 + printchar(running_x, y, *s, rgb);
    }
}

extern const uint8_t cloudysuntiny_map[];

void framebuffer::showimage(void)
{
    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint8_t index = cloudysuntiny_map[1024 + (r * 20) + c];

            (*background_rgb)[c][19 - r] = (rgb_t){
                .red = cloudysuntiny_map[(index * 4) + 2],
                .green = cloudysuntiny_map[(index * 4) + 1],
                .blue = cloudysuntiny_map[(index * 4) + 0],
            };
        }
    }
}
